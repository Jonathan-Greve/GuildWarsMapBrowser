#pragma once

#include <DirectXMath.h>
#include <vector>
#include <cstdint>
#include <cmath>
#include <stdexcept>

using namespace DirectX;

namespace GW::Parsers {

/**
 * @brief Variable Length Encoding (VLE) decoder for Guild Wars animation data.
 *
 * VLE is used to compress keyframe timing and rotation data in BB9/FA1 animation chunks.
 * This implementation is ported from animated_viewer.py and matches the game's format.
 *
 * VLE Byte Format:
 *   First byte:  [Continue:1][Sign:1][Data:6]
 *   Subsequent:  [Continue:1][Data:7]
 *
 * The continue bit (0x80) indicates more bytes follow.
 * For signed values, bit 0x40 indicates the sign.
 */
class VLEDecoder
{
public:
    VLEDecoder(const uint8_t* data, size_t dataSize, size_t offset = 0)
        : m_data(data)
        , m_dataSize(dataSize)
        , m_offset(offset)
    {
    }

    /**
     * @brief Gets the current offset position.
     */
    size_t GetOffset() const { return m_offset; }

    /**
     * @brief Sets the current offset position.
     */
    void SetOffset(size_t offset) { m_offset = offset; }

    /**
     * @brief Checks if there's more data to read.
     */
    bool HasMoreData() const { return m_offset < m_dataSize; }

    /**
     * @brief Gets remaining bytes in the buffer.
     */
    size_t RemainingBytes() const { return m_offset < m_dataSize ? m_dataSize - m_offset : 0; }

    /**
     * @brief Reads a single byte and advances the offset.
     */
    uint8_t ReadByte()
    {
        if (m_offset >= m_dataSize)
        {
            throw std::runtime_error("VLEDecoder: Unexpected end of data at offset " + std::to_string(m_offset));
        }
        return m_data[m_offset++];
    }

    /**
     * @brief Reads a single VLE-encoded unsigned value.
     *
     * Format:
     *   First byte: [Continue:1][Sign:1][Value:6]
     *   Next bytes: [Continue:1][Value:7]
     *
     * @param outSign If provided, receives the sign bit (0x40 from first byte).
     * @return The decoded unsigned value.
     */
    uint32_t ReadVLEValue(bool* outSign = nullptr)
    {
        uint8_t b = ReadByte();
        uint32_t value = b & 0x3F;

        if (outSign)
        {
            *outSign = (b & 0x40) != 0;
        }

        if (b & 0x80)
        {
            b = ReadByte();
            value |= (b & 0x7F) << 6;

            if (b & 0x80)
            {
                b = ReadByte();
                value |= (b & 0x7F) << 13;

                if (b & 0x80)
                {
                    b = ReadByte();
                    value |= (b & 0x7F) << 20;

                    if (b & 0x80)
                    {
                        b = ReadByte();
                        value |= static_cast<uint32_t>(b) << 27;
                    }
                }
            }
        }

        return value;
    }

    /**
     * @brief Decodes unsigned delta-of-delta VLE values (for keyframe times).
     *
     * Formula: new_value = (2 * prev1 - prev2) + delta
     *
     * This encoding exploits the fact that keyframe times typically increase
     * at a roughly constant rate, so the delta-of-delta is often small.
     *
     * @param count Number of values to decode.
     * @return Vector of decoded unsigned values.
     */
    std::vector<uint32_t> ExpandUnsignedDeltaVLE(uint32_t count)
    {
        std::vector<uint32_t> values;
        values.reserve(count);

        int32_t last1 = 0;
        int32_t last2 = 0;

        for (uint32_t i = 0; i < count; i++)
        {
            bool signPositive;
            uint32_t rawValue = ReadVLEValue(&signPositive);

            int32_t delta = signPositive ? static_cast<int32_t>(rawValue) : -static_cast<int32_t>(rawValue);
            int32_t newValue = (last1 * 2 - last2) + delta;

            values.push_back(static_cast<uint32_t>(newValue));

            last2 = last1;
            last1 = newValue;
        }

        return values;
    }

    /**
     * @brief Decodes a single signed delta VLE value (for Euler angle components).
     *
     * @param previous The previous value in the sequence.
     * @return The new value after applying the delta.
     */
    int16_t ExpandSignedDeltaVLE(int16_t previous)
    {
        uint8_t b = ReadByte();
        uint32_t value = b & 0x3F;
        bool signSubtract = (b & 0x40) != 0;

        if (b & 0x80)
        {
            b = ReadByte();
            value |= (b & 0x7F) << 6;

            if (b & 0x80)
            {
                b = ReadByte();
                value |= static_cast<uint32_t>(b) << 13;
            }
        }

        if (signSubtract)
        {
            return static_cast<int16_t>((previous - value) & 0xFFFF);
        }
        else
        {
            return static_cast<int16_t>((previous + value) & 0xFFFF);
        }
    }

    /**
     * @brief Reads count*3 floats as vec3 positions.
     *
     * Converts from Z-up to Y-up: (x, y, z) -> (x, -z, y)
     *
     * @param count Number of vec3 positions to read.
     * @return Vector of XMFLOAT3 positions.
     */
    std::vector<XMFLOAT3> ReadFloat3s(uint32_t count)
    {
        std::vector<XMFLOAT3> result;
        result.reserve(count);

        for (uint32_t i = 0; i < count; i++)
        {
            if (m_offset + 12 > m_dataSize)
            {
                throw std::runtime_error("VLEDecoder: Not enough data for float3 at offset " + std::to_string(m_offset));
            }

            float x, y, z;
            std::memcpy(&x, &m_data[m_offset], sizeof(float));
            std::memcpy(&y, &m_data[m_offset + 4], sizeof(float));
            std::memcpy(&z, &m_data[m_offset + 8], sizeof(float));
            m_offset += 12;

            // Convert from GW to display coords: (x, y, z) -> (x, -z, y)
            // GW uses (left/right, front/back, down/up), GWMB uses (left/right, up/down, front/back)
            result.push_back({x, -z, y});
        }

        return result;
    }

    /**
     * @brief Decompresses rotation keyframes from Euler angles to quaternions.
     *
     * Rotation data is stored as VLE-encoded delta Euler angles.
     * Scale: angle = value * (2π/65536) - π
     * Rotation order: ZYX (applied as Z, then Y, then X)
     *
     * Coordinate transform: GW uses (x, y, z) = (left/right, front/back, down/up)
     * GWMB uses (x, y, z) = (left/right, up/down, front/back)
     * Transform: (x_gwmb, y_gwmb, z_gwmb) = (x_gw, -z_gw, y_gw)
     *
     * For quaternions, the rotation axis must also be transformed:
     * (qx_gwmb, qy_gwmb, qz_gwmb, qw_gwmb) = (qx_gw, -qz_gw, qy_gw, qw_gw)
     *
     * @param count Number of rotation keyframes to decompress.
     * @return Vector of XMFLOAT4 quaternions (x,y,z,w members).
     */
    std::vector<XMFLOAT4> DecompressQuaternionKeys(uint32_t count)
    {
        std::vector<XMFLOAT4> quaternions;
        quaternions.reserve(count);

        constexpr float ANGLE_SCALE = (2.0f * 3.14159265358979323846f) / 65536.0f;
        constexpr float ANGLE_OFFSET = 3.14159265358979323846f;

        int16_t prevX = 0, prevY = 0, prevZ = 0;

        for (uint32_t i = 0; i < count; i++)
        {
            // Decode delta-encoded Euler angles (in GW coordinate space)
            prevX = ExpandSignedDeltaVLE(prevX);
            prevY = ExpandSignedDeltaVLE(prevY);
            prevZ = ExpandSignedDeltaVLE(prevZ);

            // Convert from 16-bit encoded values to radians (still in GW space)
            // GW uses TRANSPOSED rotation matrices (Ghidra: Model_DecompressQuaternionKeys @ 0x00770e60)
            // Transposed matrices = inverse rotation = rotation by negative angle
            // Therefore we negate ALL three Euler angles to match GW's convention
            float rx_gw = -(prevX * ANGLE_SCALE - ANGLE_OFFSET);
            float ry_gw = -(prevY * ANGLE_SCALE - ANGLE_OFFSET);
            float rz_gw = -(prevZ * ANGLE_SCALE - ANGLE_OFFSET);

            // Convert Euler angles to quaternion in GW coordinate space
            XMFLOAT4 quat_gw = EulerToQuaternion(rx_gw, ry_gw, rz_gw);

            // Transform quaternion from GW space to GWMB space
            // Position transform: (x, y, z) -> (x, -z, y)
            // Quaternion axis transform: (qx, qy, qz) -> (qx, -qz, qy)
            XMFLOAT4 quat;
            quat.x = quat_gw.x;
            quat.y = -quat_gw.z;
            quat.z = quat_gw.y;
            quat.w = quat_gw.w;

            // Ensure quaternion continuity (flip if dot product is negative)
            if (i > 0)
            {
                const XMFLOAT4& prev = quaternions.back();
                float dot = quat.w * prev.w + quat.x * prev.x + quat.y * prev.y + quat.z * prev.z;
                if (dot < 0)
                {
                    quat.w = -quat.w;
                    quat.x = -quat.x;
                    quat.y = -quat.y;
                    quat.z = -quat.z;
                }
            }

            quaternions.push_back(quat);
        }

        return quaternions;
    }

    /**
     * @brief Converts Euler angles (ZYX order) to quaternion.
     *
     * GW uses ZYX rotation order: Z is applied first, then Y, then X.
     * The quaternion multiplication order is reversed: q = Qx * Qy * Qz
     *
     * @param rx Rotation around X axis in radians.
     * @param ry Rotation around Y axis in radians.
     * @param rz Rotation around Z axis in radians.
     * @return Quaternion as XMFLOAT4 (x,y,z,w members where w is scalar).
     */
    static XMFLOAT4 EulerToQuaternion(float rx, float ry, float rz)
    {
        float cx = std::cos(rx * 0.5f);
        float sx = std::sin(rx * 0.5f);
        float cy = std::cos(ry * 0.5f);
        float sy = std::sin(ry * 0.5f);
        float cz = std::cos(rz * 0.5f);
        float sz = std::sin(rz * 0.5f);

        // ZYX order: q = Qx * Qy * Qz (intrinsic rotations)
        // Derived from Hamilton product: Qx * (Qy * Qz)
        XMFLOAT4 q;
        q.w = cx * cy * cz - sx * sy * sz;
        q.x = sx * cy * cz + cx * sy * sz;
        q.y = cx * sy * cz - sx * cy * sz;
        q.z = cx * cy * sz + sx * sy * cz;

        // Normalize
        float length = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
        if (length > 0.0001f)
        {
            float invLen = 1.0f / length;
            q.w *= invLen;
            q.x *= invLen;
            q.y *= invLen;
            q.z *= invLen;
        }

        return q;
    }

    /**
     * @brief Normalized Linear Interpolation (NLERP) between two quaternions.
     *
     * Guild Wars uses NLERP instead of true SLERP for quaternion interpolation.
     * NLERP is faster (no trig functions) and produces nearly identical results
     * for small angular differences between keyframes.
     *
     * Algorithm (from Ghidra RE of Gw.exe Quat_Slerp at 0x00758430):
     * 1. Compute dot product to check if quaternions are on same hemisphere
     * 2. If dot < 0, negate q2 to take shorter path (antipodal handling)
     * 3. Linear interpolation: result = (1-t)*q1 + t*q2
     * 4. Normalize the result
     *
     * @param q1 First quaternion.
     * @param q2 Second quaternion.
     * @param t Interpolation factor [0, 1].
     * @return Interpolated quaternion.
     */
    static XMFLOAT4 QuaternionSlerp(const XMFLOAT4& q1, XMFLOAT4 q2, float t)
    {
        // Compute dot product (GW computes: w1*w2 + z1*z2 + x1*x2 + y1*y2)
        float dot = q1.w * q2.w + q1.x * q2.x + q1.y * q2.y + q1.z * q2.z;

        // If dot is negative, negate q2 to take shorter path (antipodal handling)
        float sign = 1.0f;
        if (dot < 0.0f)
        {
            sign = -1.0f;
            dot = -dot;
        }

        // Linear interpolation: result = (1-t)*q1 + sign*t*q2
        float oneMinusT = 1.0f - t;
        float tSigned = sign * t;

        XMFLOAT4 result;
        result.x = oneMinusT * q1.x + tSigned * q2.x;
        result.y = oneMinusT * q1.y + tSigned * q2.y;
        result.z = oneMinusT * q1.z + tSigned * q2.z;
        result.w = oneMinusT * q1.w + tSigned * q2.w;

        // Normalize the result
        float lengthSq = result.x * result.x + result.y * result.y +
                         result.z * result.z + result.w * result.w;
        float invLen = 1.0f / std::sqrt(lengthSq);
        result.x *= invLen;
        result.y *= invLen;
        result.z *= invLen;
        result.w *= invLen;

        return result;
    }

    /**
     * @brief Rotates a point by a quaternion.
     *
     * @param q Quaternion to rotate by (XMFLOAT4 with x,y,z,w members).
     * @param p Point to rotate.
     * @return Rotated point.
     */
    static XMFLOAT3 QuaternionRotatePoint(const XMFLOAT4& q, const XMFLOAT3& p)
    {
        // Convert point to quaternion: scalar=0, vector=(px, py, pz)
        // XMFLOAT4 member order is {x, y, z, w}
        XMFLOAT4 pq = {p.x, p.y, p.z, 0.0f};

        // Compute q * p * q^-1 (for unit quaternion, conjugate = inverse)
        // Conjugate: negate vector part, keep scalar
        XMFLOAT4 qConj = {-q.x, -q.y, -q.z, q.w};

        // q * p
        XMFLOAT4 qp = QuaternionMultiply(q, pq);

        // (q * p) * q^-1
        XMFLOAT4 result = QuaternionMultiply(qp, qConj);

        return {result.x, result.y, result.z};
    }

    /**
     * @brief Multiplies two quaternions.
     *
     * @param q1 First quaternion (XMFLOAT4 with x,y,z,w members).
     * @param q2 Second quaternion (XMFLOAT4 with x,y,z,w members).
     * @return Product quaternion.
     */
    static XMFLOAT4 QuaternionMultiply(const XMFLOAT4& q1, const XMFLOAT4& q2)
    {
        XMFLOAT4 result;
        result.w = q1.w * q2.w - q1.x * q2.x - q1.y * q2.y - q1.z * q2.z;
        result.x = q1.w * q2.x + q1.x * q2.w + q1.y * q2.z - q1.z * q2.y;
        result.y = q1.w * q2.y - q1.x * q2.z + q1.y * q2.w + q1.z * q2.x;
        result.z = q1.w * q2.z + q1.x * q2.y - q1.y * q2.x + q1.z * q2.w;
        return result;
    }

    /**
     * @brief Computes the inverse of a unit quaternion (conjugate).
     *
     * For unit quaternions, inverse = conjugate (negate vector part, keep scalar).
     *
     * @param q Unit quaternion (XMFLOAT4 with x,y,z,w members).
     * @return Inverse quaternion.
     */
    static XMFLOAT4 QuaternionInverse(const XMFLOAT4& q)
    {
        return {-q.x, -q.y, -q.z, q.w};
    }

private:
    const uint8_t* m_data;
    size_t m_dataSize;
    size_t m_offset;
};

} // namespace GW::Parsers
