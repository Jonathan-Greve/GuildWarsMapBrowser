#pragma once
#include <unordered_map>
#include <memory>
#include <DirectXMath.h>
#include "RenderCommand.h"
#include "RenderBatch.h"
#include "Box.h"
#include "Sphere.h"
#include "Line.h"
#include "RenderConstants.h"
#include "PixelShader.h"
#include "BlendStateManager.h"
#include "RasterizerStateManager.h"
#include "DepthStencilStateManager.h"
#include <Dome.h>
#include <Cylinder.h>
#include <GWSkyCylinder.h>
#include <Trapezoid3D.h>
#include <Triangle3D.h>
#include <GWSkyCircle.h>

enum class RenderSelectionState {
	All,
	OpaqueOnly,
	TransparentOnly,
};

class MeshManager
{
public:
	MeshManager(ID3D11Device* device, ID3D11DeviceContext* deviceContext)
		: m_device(device)
		  , m_deviceContext(deviceContext)
		  , m_nextMeshID(0)
		  , m_needsUpdate(false)
	{
		D3D11_BUFFER_DESC cbDesc = {};
		cbDesc.Usage = D3D11_USAGE_DYNAMIC;
		cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		cbDesc.ByteWidth = sizeof(PerObjectCB);
		cbDesc.StructureByteStride = sizeof(PerObjectCB);
		m_device->CreateBuffer(&cbDesc, nullptr, m_perObjectCB.GetAddressOf());

		// Set the constant buffer for the vertex shader
		ID3D11Buffer* constantBuffers[] = {m_perObjectCB.Get()};
		m_deviceContext->VSSetConstantBuffers(PER_OBJECT_CB_SLOT, 1, constantBuffers);
		m_deviceContext->PSSetConstantBuffers(PER_OBJECT_CB_SLOT, 1, constantBuffers);
	}

	int AddBox(const XMFLOAT3& size, PixelShaderType pixel_shader_type = PixelShaderType::OldModel)
	{
		int meshID = m_nextMeshID++;
		auto mesh_instance = std::make_shared<Box>(m_device, size, meshID);
		add_to_triangle_meshes(mesh_instance, pixel_shader_type);
		m_needsUpdate = true;
		return meshID;
	}

	int AddSphere(float radius, uint32_t numSlices, uint32_t numStacks,
	              PixelShaderType pixel_shader_type = PixelShaderType::OldModel)
	{
		int meshID = m_nextMeshID++;
		auto mesh_instance = std::make_shared<Sphere>(m_device, radius, numSlices, numStacks, meshID);
		add_to_triangle_meshes(mesh_instance, pixel_shader_type);
		m_needsUpdate = true;
		return meshID;
	}

	int AddDome(float radius, uint32_t numSlices, uint32_t numStacks,
		PixelShaderType pixel_shader_type = PixelShaderType::OldModel)
	{
		int meshID = m_nextMeshID++;
		auto mesh_instance = std::make_shared<Dome>(m_device, radius, numSlices, numStacks, meshID);
		add_to_triangle_meshes(mesh_instance, pixel_shader_type);
		m_needsUpdate = true;
		return meshID;
	}

	int AddCylinder(float radius, float height, uint32_t numSlices, uint32_t numStacks,
		PixelShaderType pixel_shader_type = PixelShaderType::OldModel)
	{
		int meshID = m_nextMeshID++;
		auto mesh_instance = std::make_shared<Cylinder>(m_device, radius, height, numSlices, numStacks, meshID);
		add_to_triangle_meshes(mesh_instance, pixel_shader_type);
		m_needsUpdate = true;
		return meshID;
	}

	int AddTrapezoid3D(const Vertex3& TL, const Vertex3& TR, const Vertex3& BL, const Vertex3& BR,
		float height, PixelShaderType pixel_shader_type = PixelShaderType::OldModel)
	{
		int meshID = m_nextMeshID++;
		auto mesh_instance = std::make_shared<Trapezoid3D>(m_device, TL, TR, BL, BR, height, meshID);
		add_to_triangle_meshes(mesh_instance, pixel_shader_type);
		m_needsUpdate = true;
		return meshID;
	}

	int AddTriangle3D(const Vertex3& a, const Vertex3& b, const Vertex3& c,
		float height, PixelShaderType pixel_shader_type = PixelShaderType::OldModel)
	{
		int meshID = m_nextMeshID++;
		auto mesh_instance = std::make_shared<Triangle3D>(m_device, a, b, c, height, meshID);
		add_to_triangle_meshes(mesh_instance, pixel_shader_type);
		m_needsUpdate = true;
		return meshID;
	}

	int AddLine(const XMFLOAT3& start, const XMFLOAT3& end,
	            PixelShaderType pixel_shader_type = PixelShaderType::OldModel)
	{
		int meshID = m_nextMeshID++;
		m_lineMeshes[meshID] = std::make_shared<Line>(m_device, start, end, meshID);

		RenderCommand command = {m_lineMeshes[meshID], D3D11_PRIMITIVE_TOPOLOGY_LINELIST, pixel_shader_type};
		m_renderBatch.AddCommand(command);

		m_needsUpdate = true;
		return meshID;
	}

	int AddGwSkyCylinder(float radius, float height, PixelShaderType pixel_shader_type = PixelShaderType::Sky)
	{
		int meshID = m_nextMeshID++;
		auto mesh_instance = std::make_shared<GWSkyCylinder>(m_device, meshID, radius, height);
		add_to_triangle_meshes(mesh_instance, pixel_shader_type);
		m_needsUpdate = true;
		return meshID;
	}

	int AddGwSkyCircle(float radius, PixelShaderType pixel_shader_type = PixelShaderType::Clouds)
	{
		int meshID = m_nextMeshID++;
		auto mesh_instance = std::make_shared<GWSkyCircle>(m_device, meshID, radius);
		add_to_triangle_meshes(mesh_instance, pixel_shader_type);
		m_needsUpdate = true;
		return meshID;
	}

	int AddCustomMesh(const Mesh& mesh, PixelShaderType pixel_shader_type = PixelShaderType::OldModel)
	{
		int meshID = m_nextMeshID++;
		auto mesh_instance = std::make_shared<MeshInstance>(m_device, mesh, meshID);
		add_to_triangle_meshes(mesh_instance, pixel_shader_type);
		m_needsUpdate = true;
		return meshID;
	}

	int AddCustomMesh(const Mesh* mesh, PixelShaderType pixel_shader_type = PixelShaderType::OldModel)
	{
		int meshID = m_nextMeshID++;
		auto mesh_instance = std::make_shared<MeshInstance>(m_device, *mesh, meshID);
		add_to_triangle_meshes(mesh_instance, pixel_shader_type);
		m_needsUpdate = true;
		return meshID;
	}

	bool ChangeMeshPixelShaderType(int mesh_id, PixelShaderType pixel_shader_type)
	{
		bool found = false;

		// Iterate over the triangle meshes
		auto it = m_triangleMeshes.find(mesh_id);
		if (it != m_triangleMeshes.end()) { found = true; }
		else
		{
			// If not found in triangle meshes, search in line meshes
			it = m_lineMeshes.find(mesh_id);
			if (it != m_lineMeshes.end()) { found = true; }
		}

		if (found)
		{
			// Update the RenderCommand in the RenderBatch
			return m_renderBatch.SetPixelShader(mesh_id, pixel_shader_type);
		}

		return false;
	}

	bool RemoveMesh(int meshID)
	{
		auto it = m_triangleMeshes.find(meshID);
		if (it != m_triangleMeshes.end())
		{
			m_triangleMeshes.erase(it);
			m_renderBatch.RemoveCommand(meshID);
			m_needsUpdate = true;
			return true;
		}

		it = m_lineMeshes.find(meshID);
		if (it != m_lineMeshes.end())
		{
			m_lineMeshes.erase(it);
			m_renderBatch.RemoveCommand(meshID);
			m_needsUpdate = true;
			return true;
		}

		return false;
	}

	std::shared_ptr<MeshInstance> GetMesh(int meshID)
	{
		auto it = m_triangleMeshes.find(meshID);
		if (it != m_triangleMeshes.end()) { return it->second; }

		it = m_lineMeshes.find(meshID);
		if (it != m_lineMeshes.end()) { return it->second; }

		return nullptr;
	}

	void SetTexturesForMesh(int meshID, const std::vector<ID3D11ShaderResourceView*>& textures, int slot)
	{
		auto it = m_triangleMeshes.find(meshID);
		if (it != m_triangleMeshes.end()) { it->second->SetTextures(textures, slot); }
	}

	void UpdateMeshVertices(int meshID, const std::vector<GWVertex>& vertices)
	{
		auto it = m_triangleMeshes.find(meshID);
		if (it != m_triangleMeshes.end())
		{
			it->second->UpdateVertices(m_device, vertices);
		}
	}

	void UpdateMeshPerObjectData(int meshID, const PerObjectCB& data)
	{
		auto it = m_triangleMeshes.find(meshID);
		if (it != m_triangleMeshes.end())
		{
			it->second->SetPerObjectData(data);
			return;
		}

		it = m_lineMeshes.find(meshID);
		if (it != m_lineMeshes.end()) { it->second->SetPerObjectData(data); }
	}

	std::optional<PerObjectCB> GetMeshPerObjectData(int mesh_id) {
		auto it = m_triangleMeshes.find(mesh_id);
		if (it != m_triangleMeshes.end())
		{
			return it->second->GetPerObjectData();
		}

		return std::nullopt;
	}

	void SetMeshPosition(int mesh_id, float x, float y, float z)
	{
		std::shared_ptr<MeshInstance> mesh = nullptr;

		auto it = m_triangleMeshes.find(mesh_id);
		if (it != m_triangleMeshes.end())
		{
			mesh = it->second;
		}
		else
		{
			auto lineIt = m_lineMeshes.find(mesh_id);
			if (lineIt != m_lineMeshes.end())
			{
				mesh = lineIt->second;
			}
		}

		if (mesh)
		{
			PerObjectCB data = mesh->GetPerObjectData();
			// Create translation matrix
			DirectX::XMMATRIX translationMatrix = DirectX::XMMatrixTranslation(x, y, z);
			DirectX::XMStoreFloat4x4(&data.world, translationMatrix);
			mesh->SetPerObjectData(data);
		}
	}

	void SetMeshColor(int mesh_id, const DirectX::XMFLOAT4& color)
	{
		std::shared_ptr<MeshInstance> mesh = nullptr;

		auto it = m_triangleMeshes.find(mesh_id);
		if (it != m_triangleMeshes.end())
		{
			mesh = it->second;
		}
		else
		{
			auto lineIt = m_lineMeshes.find(mesh_id);
			if (lineIt != m_lineMeshes.end())
			{
				mesh = lineIt->second;
			}
		}

		if (mesh)
		{
			PerObjectCB data = mesh->GetPerObjectData();
			data.object_color = color;
			mesh->SetPerObjectData(data);
		}
	}

	bool SetMeshShouldRender(int mesh_id, bool should_render)
	{
		bool found = false;

		// Iterate over the triangle meshes
		auto it = m_triangleMeshes.find(mesh_id);
		if (it != m_triangleMeshes.end()) { found = true; }
		else
		{
			// If not found in triangle meshes, search in line meshes
			it = m_lineMeshes.find(mesh_id);
			if (it != m_lineMeshes.end()) { found = true; }
		}

		if (found)
		{
			// Call SetShouldRender function in the RenderBatch
			m_renderBatch.SetShouldRender(mesh_id, should_render);
		}

		return found;
	}

	bool GetMeshShouldRender(int mesh_id)
	{
		auto command = m_renderBatch.GetCommand(mesh_id);
		if (command.has_value()) { return command->should_render; }

		return false;
	}

	bool SetMeshShouldCull(int mesh_id, bool should_cull)
	{
		bool found = false;

		// Iterate over the triangle meshes
		auto it = m_triangleMeshes.find(mesh_id);
		if (it != m_triangleMeshes.end()) { found = true; }
		else
		{
			// If not found in triangle meshes, search in line meshes
			it = m_lineMeshes.find(mesh_id);
			if (it != m_lineMeshes.end()) { found = true; }
		}

		if (found)
		{
			// Call SetShouldRender function in the RenderBatch
			it->second->SetShouldCull(should_cull);
			m_renderBatch.SetShouldCull(mesh_id, should_cull);
		}

		return found;
	}

	bool GetMeshShouldCull(int mesh_id)
	{
		auto command = m_renderBatch.GetCommand(mesh_id);
		if (command.has_value()) { return command->should_cull; }

		return false;
	}

	unsigned int DecodeObjectID(unsigned char* data, int x, int y, int rowPitch)
	{
		// Calculate the offset to the pixel at (x, y)
		size_t offset = (y * rowPitch) + (x * 4); // Assume each pixel takes up 4 bytes (RGBA)

		// Extract the color components
		unsigned char r = data[offset];
		unsigned char g = data[offset + 1];
		unsigned char b = data[offset + 2];

		// Reconstruct the object ID from the RGB channels
		unsigned int objectID = (static_cast<unsigned int>(r) << 16) |
		(static_cast<unsigned int>(g) << 8) |
		static_cast<unsigned int>(b);

		return objectID;
	}

	int GetPickedObjectId(ID3D11DeviceContext* context, ID3D11Texture2D* offscreenTexture, int x, int y)
    {
        // Validate coordinates
        D3D11_TEXTURE2D_DESC desc;
        offscreenTexture->GetDesc(&desc);

        if (x < 0 || x >= static_cast<int>(desc.Width) || y < 0 || y >= static_cast<int>(desc.Height)) 
        {
            return -1; // Invalid coordinates
        }

        // Read back the texture data
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        HRESULT hr = context->Map(offscreenTexture, 0, D3D11_MAP_READ, 0, &mappedResource);

        if (FAILED(hr))
        {
            // Log or print HRESULT error
            return -1; // Invalid object ID
        }

        auto data = static_cast<unsigned char*>(mappedResource.pData);
        unsigned int objectID = DecodeObjectID(data, x, y, mappedResource.RowPitch);

        context->Unmap(offscreenTexture, 0);

        return objectID;
	}

	void Update(float dt)
	{
		if (m_needsUpdate) { m_needsUpdate = false; }
	}

	/**
	 * @brief Sets the per-object constant buffer data directly.
	 *
	 * Use this for skinned mesh rendering where we need to set per-object data
	 * without going through the normal render batch.
	 *
	 * @param data Per-object constant buffer data (world matrix should already be transposed).
	 */
	void SetPerObjectCB(const PerObjectCB& data)
	{
		D3D11_MAPPED_SUBRESOURCE mappedResource;
		m_deviceContext->Map(m_perObjectCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		memcpy(mappedResource.pData, &data, sizeof(PerObjectCB));
		m_deviceContext->Unmap(m_perObjectCB.Get(), 0);
	}

	// Render only a specific mesh. Ignores should_render.
	void RenderMesh(std::unordered_map<PixelShaderType, std::unique_ptr<PixelShader>>& pixel_shaders,
		BlendStateManager* blend_state_manager, RasterizerStateManager* rasterizer_state_manager,
		DepthStencilStateManager* depth_stencil_state_manager, XMFLOAT3 camera_position, LODQuality lod_quality, 
		int mesh_id, RenderSelectionState render_select_state = RenderSelectionState::All, bool should_set_ps = true,
		bool should_overwrite_shader = false, PixelShaderType overwrite_shader = PixelShaderType::OldModel) {

		auto command = m_renderBatch.GetCommand(mesh_id);
		if (command.has_value()) {
			if (render_select_state != RenderSelectionState::All) {
				// Note that these blend_states are estimates. I don't know for sure which ones are transparent until after the models pixel shader has run.
				if (command->blend_state == BlendState::Opaque && render_select_state != RenderSelectionState::OpaqueOnly) {
					return;
				}
				if (command->blend_state == BlendState::AlphaBlend && render_select_state != RenderSelectionState::TransparentOnly) {
					return;
				}
			}

			m_deviceContext->IASetPrimitiveTopology(command->primitiveTopology);

			if (should_overwrite_shader) {
				m_deviceContext->PSSetShader(pixel_shaders[overwrite_shader]->GetShader(), nullptr, 0);
			}
			else if (should_set_ps) {
				m_deviceContext->PSSetShader(pixel_shaders[command->pixelShaderType]->GetShader(), nullptr, 0);
			}
			m_deviceContext->PSSetSamplers(0, 1, pixel_shaders[command->pixelShaderType]->GetSamplerState());
			m_deviceContext->PSSetSamplers(1, 1, pixel_shaders[command->pixelShaderType]->GetSamplerStateShadow());


			if (command->should_cull) { rasterizer_state_manager->SetRasterizerState(RasterizerStateType::Solid); }
			else { rasterizer_state_manager->SetRasterizerState(RasterizerStateType::Solid_NoCull); }


			blend_state_manager->SetBlendState(BlendState::AlphaBlend);

			PerObjectCB transposedData = command->meshInstance->GetPerObjectData();

			// Load World matrix into an XMMATRIX, transpose it, and store it back into an XMFLOAT4X4
			XMMATRIX worldMatrix = XMLoadFloat4x4(&transposedData.world);
			worldMatrix = XMMatrixTranspose(worldMatrix);
			XMStoreFloat4x4(&transposedData.world, worldMatrix);

			// Update the per object constant buffer
			D3D11_MAPPED_SUBRESOURCE mappedResource;
			m_deviceContext->Map(m_perObjectCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
			memcpy(mappedResource.pData, &transposedData, sizeof(PerObjectCB));
			m_deviceContext->Unmap(m_perObjectCB.Get(), 0);

			command->meshInstance->Draw(m_deviceContext, lod_quality);
		}
	}
		

	void Render(std::unordered_map<PixelShaderType, std::unique_ptr<PixelShader>>& pixel_shaders,
	            BlendStateManager* blend_state_manager, RasterizerStateManager* rasterizer_state_manager,
	            DepthStencilStateManager* depth_stencil_state_manager, XMFLOAT3 camera_position, LODQuality lod_quality,
				RenderSelectionState render_select_state = RenderSelectionState::All, bool should_set_ps = true,
				bool should_overwrite_old_model_shader = false, PixelShaderType overwrite_old_model_shader = PixelShaderType::OldModel,
				bool should_overwrite_new_model_shader = false, PixelShaderType overwrite_new_model_shader = PixelShaderType::NewModel,
				bool wireframe = false
		)
	{
		static D3D11_PRIMITIVE_TOPOLOGY currentTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
		
		static XMFLOAT3 prev_camera_position{ 0,0,0 };

		m_renderBatch.SortCommands(camera_position);

		blend_state_manager->SetBlendState(BlendState::AlphaBlend);

		for (const RenderCommand& command : m_renderBatch.GetCommands())
		{
			if (!command.should_render)
				continue;
			if (render_select_state != RenderSelectionState::All) {
				if (command.blend_state == BlendState::Opaque && render_select_state != RenderSelectionState::OpaqueOnly) {
					continue;
				}
				if (command.blend_state == BlendState::AlphaBlend && render_select_state != RenderSelectionState::TransparentOnly) {
					continue;
				}
			}

			if (command.primitiveTopology != currentTopology)
			{
				m_deviceContext->IASetPrimitiveTopology(command.primitiveTopology);
				currentTopology = command.primitiveTopology;
			}

			if (should_overwrite_old_model_shader && command.pixelShaderType == PixelShaderType::OldModel) {
				m_deviceContext->PSSetShader(pixel_shaders[overwrite_old_model_shader]->GetShader(), nullptr, 0);
			}
			else if (should_overwrite_new_model_shader && command.pixelShaderType == PixelShaderType::NewModel) {
				m_deviceContext->PSSetShader(pixel_shaders[overwrite_new_model_shader]->GetShader(), nullptr, 0);
			}
			else if (should_set_ps) {
				m_deviceContext->PSSetShader(pixel_shaders[command.pixelShaderType]->GetShader(), nullptr, 0);
			}
			m_deviceContext->PSSetSamplers(0, 1, pixel_shaders[command.pixelShaderType]->GetSamplerState());
			m_deviceContext->PSSetSamplers(1, 1, pixel_shaders[command.pixelShaderType]->GetSamplerStateShadow());


			if (wireframe) {
				rasterizer_state_manager->SetRasterizerState(command.should_cull ? RasterizerStateType::Wireframe : RasterizerStateType::Wireframe_NoCull);
			} else {
				rasterizer_state_manager->SetRasterizerState(command.should_cull ? RasterizerStateType::Solid : RasterizerStateType::Solid_NoCull);
			}

			PerObjectCB transposedData = command.meshInstance->GetPerObjectData();

			// Load World matrix into an XMMATRIX, transpose it, and store it back into an XMFLOAT4X4
			XMMATRIX worldMatrix = XMLoadFloat4x4(&transposedData.world);
			worldMatrix = XMMatrixTranspose(worldMatrix);
			XMStoreFloat4x4(&transposedData.world, worldMatrix);

			// Update the per object constant buffer
			D3D11_MAPPED_SUBRESOURCE mappedResource;
			m_deviceContext->Map(m_perObjectCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
			memcpy(mappedResource.pData, &transposedData, sizeof(PerObjectCB));
			m_deviceContext->Unmap(m_perObjectCB.Get(), 0);

			command.meshInstance->Draw(m_deviceContext, lod_quality);
		}
	}

private:
	ID3D11Device* m_device;
	ID3D11DeviceContext* m_deviceContext;
	int m_nextMeshID;
	bool m_needsUpdate;
	std::unordered_map<int, std::shared_ptr<MeshInstance>> m_triangleMeshes;
	std::unordered_map<int, std::shared_ptr<MeshInstance>> m_lineMeshes;
	RenderBatch m_renderBatch;

	Microsoft::WRL::ComPtr<ID3D11Buffer> m_perObjectCB;

	void add_to_triangle_meshes(std::shared_ptr<MeshInstance> mesh_instance,
	                            PixelShaderType pixel_shader_type)
	{
		m_triangleMeshes[mesh_instance->GetMeshID()] = mesh_instance;
		bool should_cull = mesh_instance->GetMesh().should_cull;
		auto blend_state = mesh_instance->GetMesh().blend_state;

		RenderCommand command = {
			mesh_instance, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
			pixel_shader_type, should_cull,
			blend_state, true
		};

		m_renderBatch.AddCommand(command);
	}
};
