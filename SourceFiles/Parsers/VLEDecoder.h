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
     * Guild Wars uses Y-up with inverted Z, so we negate Z when reading.
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

            // Match mesh coordinate transform: (x, y, z) -> (x, -z, y)
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
     * @param count Number of rotation keyframes to decompress.
     * @return Vector of XMFLOAT4 quaternions (w, x, y, z order).
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
            // Decode delta-encoded Euler angles
            prevX = ExpandSignedDeltaVLE(prevX);
            prevY = ExpandSignedDeltaVLE(prevY);
            prevZ = ExpandSignedDeltaVLE(prevZ);

            // Convert from 16-bit encoded values to radians
            float rx = prevX * ANGLE_SCALE - ANGLE_OFFSET;
            float ry = prevY * ANGLE_SCALE - ANGLE_OFFSET;
            float rz = prevZ * ANGLE_SCALE - ANGLE_OFFSET;

            // Convert Euler angles to quaternion
            XMFLOAT4 quat = EulerToQuaternion(rx, ry, rz);

            // Convert quaternion to match mesh coordinate system (swap Y and Z axes)
            // For coordinate transform (x, y, z) -> (x, -z, y), quaternion becomes (w, x, -z, y)
            XMFLOAT4 converted;
            converted.w = quat.w;
            converted.x = quat.x;
            converted.y = -quat.z;  // New Y component = -old Z
            converted.z = quat.y;   // New Z component = old Y
            quat = converted;

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
     * @brief Converts Euler angles (XYZ order) to quaternion.
     *
     * @param rx Rotation around X axis in radians.
     * @param ry Rotation around Y axis in radians.
     * @param rz Rotation around Z axis in radians.
     * @return Quaternion in (w, x, y, z) format.
     */
    static XMFLOAT4 EulerToQuaternion(float rx, float ry, float rz)
    {
        float cx = std::cos(rx * 0.5f);
        float sx = std::sin(rx * 0.5f);
        float cy = std::cos(ry * 0.5f);
        float sy = std::sin(ry * 0.5f);
        float cz = std::cos(rz * 0.5f);
        float sz = std::sin(rz * 0.5f);

        XMFLOAT4 q;
        q.w = cx * cy * cz + sx * sy * sz;
        q.x = sx * cy * cz - cx * sy * sz;
        q.y = cx * sy * cz + sx * cy * sz;
        q.z = cx * cy * sz - sx * sy * cz;

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
     * @brief Spherical linear interpolation between two quaternions.
     *
     * @param q1 First quaternion.
     * @param q2 Second quaternion.
     * @param t Interpolation factor [0, 1].
     * @return Interpolated quaternion.
     */
    static XMFLOAT4 QuaternionSlerp(const XMFLOAT4& q1, XMFLOAT4 q2, float t)
    {
        // Compute dot product
        float dot = q1.w * q2.w + q1.x * q2.x + q1.y * q2.y + q1.z * q2.z;

        // If dot is negative, negate one quaternion to take shorter path
        if (dot < 0.0f)
        {
            q2.w = -q2.w;
            q2.x = -q2.x;
            q2.y = -q2.y;
            q2.z = -q2.z;
            dot = -dot;
        }

        // If quaternions are very close, use linear interpolation
        if (dot > 0.9995f)
        {
            XMFLOAT4 result;
            result.w = q1.w + t * (q2.w - q1.w);
            result.x = q1.x + t * (q2.x - q1.x);
            result.y = q1.y + t * (q2.y - q1.y);
            result.z = q1.z + t * (q2.z - q1.z);

            // Normalize
            float length = std::sqrt(result.w * result.w + result.x * result.x +
                                     result.y * result.y + result.z * result.z);
            float invLen = 1.0f / length;
            result.w *= invLen;
            result.x *= invLen;
            result.y *= invLen;
            result.z *= invLen;

            return result;
        }

        // Standard slerp
        float theta = std::acos(dot);
        float sinTheta = std::sin(theta);
        float s1 = std::sin((1.0f - t) * theta) / sinTheta;
        float s2 = std::sin(t * theta) / sinTheta;

        XMFLOAT4 result;
        result.w = s1 * q1.w + s2 * q2.w;
        result.x = s1 * q1.x + s2 * q2.x;
        result.y = s1 * q1.y + s2 * q2.y;
        result.z = s1 * q1.z + s2 * q2.z;

        return result;
    }

    /**
     * @brief Rotates a point by a quaternion.
     *
     * @param q Quaternion to rotate by (w, x, y, z format).
     * @param p Point to rotate.
     * @return Rotated point.
     */
    static XMFLOAT3 QuaternionRotatePoint(const XMFLOAT4& q, const XMFLOAT3& p)
    {
        // Convert point to quaternion (0, px, py, pz)
        XMFLOAT4 pq = {0.0f, p.x, p.y, p.z};

        // Compute q * p * q^-1 (for unit quaternion, conjugate = inverse)
        XMFLOAT4 qConj = {q.w, -q.x, -q.y, -q.z};

        // q * p
        XMFLOAT4 qp = QuaternionMultiply(q, pq);

        // (q * p) * q^-1
        XMFLOAT4 result = QuaternionMultiply(qp, qConj);

        return {result.x, result.y, result.z};
    }

    /**
     * @brief Multiplies two quaternions.
     *
     * @param q1 First quaternion (w, x, y, z).
     * @param q2 Second quaternion (w, x, y, z).
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

private:
    const uint8_t* m_data;
    size_t m_dataSize;
    size_t m_offset;
};

} // namespace GW::Parsers
