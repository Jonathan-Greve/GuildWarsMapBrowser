#include "pch.h"
#include "draw_dat_browser.h"
#include "draw_texture_panel.h"
#include "animation_state.h"
#include "ModelViewer/ModelViewer.h"
#include "Parsers/BB9AnimationParser.h"

#include <codecvt>
#include <set>

#include "GuiGlobalConstants.h"
#include "maps_constant_data.h"
#include <numeric>
#include <set>

#include <model_exporter.h>

#include "map_exporter.h"
#include "writeHeighMapBMP.h"
#include "writeOBJ.h"

// BASS
extern LPFNBASSSTREAMCREATEFILE lpfnBassStreamCreateFile;
extern LPFNBASSCHANNELBYTES2SECONDS lpfnBassChannelBytes2Seconds;
extern LPFNBASSCHANNELGETLENGTH lpfnBassChannelGetLength;
extern LPFNBASSSTREAMGETFILEPOSITION lpfnBassStreamGetFilePosition;
extern LPFNBASSCHANNELGETINFO lpfnBassChannelGetInfo;
extern LPFNBASSCHANNELPLAY lpfnBassChannelPlay;
extern LPFNBASSCHANNELSTOP lpfnBassChannelStop;
extern LPFNBASSSTREAMFREE lpfnBassStreamFree;
extern LPFNBASSCHANNELFLAGS lpfnBassChannelFlags;
extern LPFNBASSCHANNELSETATTRIBUTE lpfnBassChannelSetAttribute;

// BASS_FX
extern LPFNBASSFXTMPOCREATE lpfnBassFxTempoCreate;

extern bool repeat_audio;
extern float playback_speed;
extern float volume_level;

extern std::string selected_text_file_str = "";

inline extern int selected_map_file_index = -1;

inline extern uint32_t selected_item_hash = -1;
inline extern uint32_t selected_item_murmurhash3 = -1;
inline int last_focused_item_index = -1;

inline extern FileType selected_file_type = NONE;
inline extern FFNA_ModelFile selected_ffna_model_file{};
inline extern FFNA_ModelFile_Other selected_ffna_model_file_other{};
inline extern bool using_other_model_format = false;
inline extern FFNA_MapFile selected_ffna_map_file{};
inline extern SelectedDatTexture selected_dat_texture{};
inline extern std::vector<uint8_t> selected_raw_data{};

inline extern std::unordered_map<uint32_t, uint32_t> object_id_to_prop_index{};
inline extern std::unordered_map<uint32_t, uint32_t> object_id_to_submodel_index{};
inline extern std::unordered_map<uint32_t, uint32_t> prop_index_to_selected_map_files_index{};

inline extern HSTREAM selected_audio_stream_handle{};
inline extern std::string audio_info = "";

inline extern std::vector<FileData> selected_map_files{};

inline bool is_parsing_data = false;
inline bool items_to_parse = false;
inline bool items_parsed = false;

inline std::unordered_map<int, TextureType> model_texture_types;

std::unique_ptr<Terrain> terrain;
std::vector<Mesh> prop_meshes;

const ImGuiTableSortSpecs* DatBrowserItem::s_current_sort_specs = nullptr;

DirectX::XMFLOAT4 GetAverageColorOfBottomRow(const DatTexture& dat_texture);
GWVertex get_shore_vertex_for_2_points(Vertex2 point1, Vertex2 point2, float height = 5);
GWVertex get_shore_vertex_for_3_points(Vertex2 point1, Vertex2 point2, Vertex2 point3, float height = 5);
void generate_shore_mesh(const XMFLOAT2& point1, const XMFLOAT2& point2, Terrain* terrain, std::vector<Mesh>& meshes, float height = 5);

void apply_filter(const std::vector<int>& new_filter, std::unordered_set<int>& intersection)
{
	if (intersection.empty()) { intersection.insert(new_filter.begin(), new_filter.end()); }
	else
	{
		std::unordered_set<int> new_intersection;
		for (int id : new_filter) { if (intersection.contains(id)) { new_intersection.insert(id); } }
		intersection = std::move(new_intersection);
	}
}

bool parse_file(DATManager* dat_manager, int index, MapRenderer* map_renderer,
	std::unordered_map<int, std::vector<int>>& hash_index)
{
	bool success = false;

	const auto MFT = dat_manager->get_MFT();
	if (index >= MFT.size())
		return false;

	const auto* entry = &MFT[index];
	if (!entry)
		return false;

	selected_file_type = static_cast<FileType>(entry->type);

	if (selected_audio_stream_handle != 0)
	{
		lpfnBassChannelStop(selected_audio_stream_handle);
		lpfnBassStreamFree(selected_audio_stream_handle);
	}

	unsigned char* raw_data = dat_manager->read_file(index);
	selected_raw_data = std::vector<uint8_t>(raw_data, raw_data + entry->uncompressedSize);
	delete raw_data;

	if (entry->type != FFNA_Type3)
	{
		// If we select a map with index 123. Then select a model we would not be able to go back to map 123 unless we did this.
		selected_map_file_index = -1;
	}

	switch (entry->type)
	{
	case TEXT:
	{
		std::string text_str(reinterpret_cast<char*>(selected_raw_data.data()));
		selected_text_file_str = text_str;
		success = true;
	}
	break;
	case SOUND:
	case AMP:
	{
		if (selected_raw_data.size() > 0)
		{
			// create the original stream
			HSTREAM orig_stream = lpfnBassStreamCreateFile(TRUE, // mem
				selected_raw_data.data(), // file
				0, // offset
				entry->uncompressedSize, // length
				BASS_STREAM_PRESCAN | BASS_STREAM_DECODE); // flags

			// create the tempo stream from the original stream
			selected_audio_stream_handle = lpfnBassFxTempoCreate(orig_stream, BASS_FX_FREESOURCE);

			float time = lpfnBassChannelBytes2Seconds(
				selected_audio_stream_handle,
				lpfnBassChannelGetLength(selected_audio_stream_handle,
					BASS_POS_BYTE)); // playback duration
			DWORD len =
				lpfnBassStreamGetFilePosition(selected_audio_stream_handle, BASS_FILEPOS_END); // file length
			DWORD bitrate = static_cast<DWORD>(len / (125 * time) + 0.5); // bitrate (Kbps)

			BASS_CHANNELINFO info;
			lpfnBassChannelGetInfo(selected_audio_stream_handle, &info);

			audio_info = "Bitrate: " + std::to_string(bitrate) +
				"\nFrequency: " + std::to_string(info.freq / 1000) + " kHz" +
				"\nChannels: " + (info.chans == 1 ? "mono" : "Stereo") +
				"\nFormat: " + (info.ctype == BASS_CTYPE_STREAM_MP3 ? "mp3" : "unknown");

			if (repeat_audio)
			{
				// Turn on looping
				lpfnBassChannelFlags(selected_audio_stream_handle, BASS_SAMPLE_LOOP, BASS_SAMPLE_LOOP);
			}

			// Adjust the tempo
			lpfnBassChannelSetAttribute(selected_audio_stream_handle, BASS_ATTRIB_TEMPO,
				(playback_speed - 1.0f) * 100.0f);

			// Set audio volume level
			lpfnBassChannelSetAttribute(selected_audio_stream_handle, BASS_ATTRIB_VOL, volume_level);

			lpfnBassChannelPlay(selected_audio_stream_handle, TRUE);

			success = true;
		}
	}

	break;
	case ATEXDXT1:
	case ATEXDXT2:
	case ATEXDXT3:
	case ATEXDXT4:
	case ATEXDXT5:
	case ATEXDXTN:
		//case ATEXDXTA: Cannot parse this
	case ATEXDXTL:
	case ATTXDXT1:
	case ATTXDXT3:
	case ATTXDXT5:
	case ATTXDXTN:
		//case ATTXDXTA: Cannot parse this
	case ATTXDXTL:
	{
		selected_dat_texture.dat_texture = dat_manager->parse_ffna_texture_file(index);
		selected_dat_texture.file_id = entry->Hash;
		if (selected_dat_texture.dat_texture.width > 0 && selected_dat_texture.dat_texture.height > 0)
		{
			map_renderer->GetTextureManager()->CreateTextureFromRGBA(
				selected_dat_texture.dat_texture.width,
				selected_dat_texture.dat_texture.height,
				selected_dat_texture.dat_texture.rgba_data.
				data(), &selected_dat_texture.texture_id,
				entry->Hash);

			success = true;
		}
	}
	break;
	case DDS:
	{
		selected_dat_texture.file_id = entry->Hash;
		const std::vector<uint8_t> ddsData = dat_manager->parse_dds_file(index);
		size_t ddsDataSize = ddsData.size();
		HRESULT hr = map_renderer->GetTextureManager()->CreateTextureFromDDSInMemory(
			ddsData.data(), ddsDataSize, &selected_dat_texture.texture_id,
			&selected_dat_texture.dat_texture.width, &selected_dat_texture.dat_texture.height,
			selected_dat_texture.dat_texture.rgba_data, entry->Hash); // Pass the RGBA vector
		if (FAILED(hr))
		{
			// Handle the error
		}
		else {
			success = true;
		}
	}
	break;
	break;
	case FFNA_Type2:
		// Fully clear previous map/model scene state before loading a standalone model.
		if (g_modelViewerState.isActive)
		{
			DeactivateModelViewer(map_renderer);
		}
		else
		{
			map_renderer->ClearCameraOverride();
		}

		// Clear up some GPU memory (especially important for GPUs with little VRAM)
		map_renderer->GetTextureManager()->Clear();
		map_renderer->ClearSceneForModeSwitch();

		// Check if this is an "other" model format (uses 0xBB* chunks instead of 0xFA*)
		using_other_model_format = dat_manager->is_other_model_format(index);

		if (using_other_model_format)
		{
			selected_ffna_model_file_other = dat_manager->parse_ffna_model_file_other(index);
		}
		else
		{
			selected_ffna_model_file = dat_manager->parse_ffna_model_file(index);
		}

		// Cancel any in-flight animation search from the previous model.
		CancelAnimationSearch();

		// Reset animation state
		g_animationState.Reset();

		if ((using_other_model_format && selected_ffna_model_file_other.parsed_correctly) ||
		    (!using_other_model_format && selected_ffna_model_file.parsed_correctly))
		{
			// Extract model hashes from geometry chunk for finding matching animations
			uint32_t modelHash0, modelHash1;
			if (using_other_model_format)
			{
				// BB8 format: hashes are in header.model_hash0/model_hash1
				modelHash0 = selected_ffna_model_file_other.geometry_chunk.header.model_hash0;
				modelHash1 = selected_ffna_model_file_other.geometry_chunk.header.model_hash1;
			}
			else
			{
				// FA0 format: hashes are in sub_1.f0xC/f0x10
				modelHash0 = selected_ffna_model_file.geometry_chunk.sub_1.f0xC;
				modelHash1 = selected_ffna_model_file.geometry_chunk.sub_1.f0x10;
			}

			// Set model hashes
			g_animationState.SetModelHashes(modelHash0, modelHash1, entry->Hash);

			// Extract FA1 bind pose parents from model file FIRST (for FA0/FA1 format)
			// These parent indices are more accurate than BB9's hierarchyByte
			// Must be done BEFORE loading animation so skeleton gets correct parents
			g_animationState.fa1BindPoseParents.clear();
			g_animationState.fa1BindPosePositions.clear();
			g_animationState.hasFA1BindPose = false;

			if (!using_other_model_format && !selected_raw_data.empty())
			{
				// Scan for FA0/FA1 chunks (animation chunks with 88-byte header and bind pose data)
				// Note: FA6 is a file reference chunk, NOT an animation chunk
				const uint8_t* data = selected_raw_data.data();
				size_t dataSize = selected_raw_data.size();

				for (size_t offset = 5; offset + 8 < dataSize; )
				{
					uint32_t chunkId = *reinterpret_cast<const uint32_t*>(&data[offset]);
					uint32_t chunkSize = *reinterpret_cast<const uint32_t*>(&data[offset + 4]);

					// Check for FA0/FA1 animation chunks (88-byte header format)
					if (chunkId == GW::Parsers::CHUNK_ID_FA0 ||
						chunkId == GW::Parsers::CHUNK_ID_FA1)
					{
						// Found FA chunk - extract bind pose data
						const uint8_t* fa1Data = &data[offset + 8]; // Skip chunk header
						size_t fa1Size = std::min(static_cast<size_t>(chunkSize), dataSize - offset - 8);

						size_t boneCount = GW::Parsers::BB9AnimationParser::ParseFA1BindPose(
							fa1Data, fa1Size,
							g_animationState.fa1BindPoseParents,
							g_animationState.fa1BindPosePositions);

						if (boneCount > 0)
						{
							g_animationState.hasFA1BindPose = true;
							char debug[128];
							sprintf_s(debug, "Extracted FA bind pose (chunk 0x%X): %zu bones\n", chunkId, boneCount);
							LogBB8Debug(debug);
						}
						break;
					}

					offset += 8 + chunkSize;
					if (chunkSize == 0) break; // Avoid infinite loop
				}
			}

			// Now try to parse animation data from the raw file
			// FA1 parents are already extracted, so LoadAnimationFromResult will use them
			char debugMsg[256];
			sprintf_s(debugMsg, "FA1 state: hasFA1=%d, parentCount=%zu\n",
				g_animationState.hasFA1BindPose ? 1 : 0,
				g_animationState.fa1BindPoseParents.size());
			LogBB8Debug(debugMsg);

			// Try to parse animation from the model file (if it has embedded animation)
			if (!selected_raw_data.empty())
			{
				auto clipOpt = GW::Parsers::ParseAnimationFromFile(selected_raw_data.data(), selected_raw_data.size());
				if (clipOpt)
				{
					auto clip = std::make_shared<GW::Animation::AnimationClip>(std::move(*clipOpt));
					// Parent indices are computed during parsing from hierarchy-byte stream
					// validity (deterministic; no 0x4000 mode switch assumption).
					auto skeleton = std::make_shared<GW::Animation::Skeleton>(
						GW::Parsers::BB9AnimationParser::CreateSkeleton(*clip));
					g_animationState.Initialize(clip, skeleton, entry->Hash);
				}
			}

			// Disable shadows for models when viewing standalone (no terrain = no shadow map)
			map_renderer->SetShouldRenderShadowsForModels(false);
			// Clear the shadow map binding from slot 0 to avoid D3D11 validation errors
			map_renderer->ClearShadowMapBinding();
			prop_meshes.clear();

			float overallMinX = FLT_MAX, overallMinY = FLT_MAX, overallMinZ = FLT_MAX;
			float overallMaxX = FLT_MIN, overallMaxY = FLT_MIN, overallMaxZ = FLT_MIN;

			// Get models from the correct model file format
			const auto& models = using_other_model_format ?
				selected_ffna_model_file_other.geometry_chunk.models :
				selected_ffna_model_file.geometry_chunk.models;

			// Set up submesh info for animation panel
			g_animationState.SetSubmeshInfo(models.size());
			g_animationState.submeshBoneData.clear();
			g_animationState.perVertexBoneGroups.clear();
			g_animationState.originalMeshes.clear();
			g_animationState.animatedMeshes.clear();
			g_animationState.hasSkinnedMeshes = false;

			// Extract bone data for each submesh
			for (size_t i = 0; i < models.size(); i++)
			{
				const auto& model = models[i];
				// u0 = bone_group_count, u1 = total_bone_refs
				auto boneData = AnimationPanelState::ExtractBoneData(model.extra_data, model.u0, model.u1);
				g_animationState.submeshBoneData.push_back(boneData);

				// Extract per-vertex bone group indices from ModelVertex.group
				std::vector<uint32_t> vertexBoneGroups;
				vertexBoneGroups.reserve(model.vertices.size());
				std::set<uint32_t> uniqueGroups;
				for (const auto& mv : model.vertices)
				{
					uint32_t grp = mv.has_group ? mv.group : 0;
					vertexBoneGroups.push_back(grp);
					uniqueGroups.insert(grp);
				}
				g_animationState.perVertexBoneGroups.push_back(std::move(vertexBoneGroups));

				// Debug: log bone mapping info
				char debug[512];
				sprintf_s(debug, "Submesh %zu: u0(boneGroupCount)=%u, u1(totalBoneRefs)=%u, "
					"groupSizes.size=%zu, skelBoneIndices.size=%zu, groupMapping.size=%zu\n",
					i, model.u0, model.u1,
					boneData.groupSizes.size(),
					boneData.skeletonBoneIndices.size(),
					boneData.groupToSkeletonBone.size());
				LogBB8Debug(debug);

				sprintf_s(debug, "  Unique vertex bone groups: %zu (range: %u to %u)\n",
					uniqueGroups.size(),
					uniqueGroups.empty() ? 0 : *uniqueGroups.begin(),
					uniqueGroups.empty() ? 0 : *uniqueGroups.rbegin());
				LogBB8Debug(debug);

				// Show first few skeleton bone mappings
				if (!boneData.groupToSkeletonBone.empty())
				{
					std::string mappings = "  Group->SkelBone: ";
					for (size_t j = 0; j < std::min(size_t(10), boneData.groupToSkeletonBone.size()); j++)
					{
						mappings += std::to_string(j) + "->" + std::to_string(boneData.groupToSkeletonBone[j]) + " ";
					}
					if (boneData.groupToSkeletonBone.size() > 10)
						mappings += "...";
					mappings += "\n";
					LogBB8Debug(mappings.c_str());
				}
				else
				{
					LogBB8Debug("  WARNING: No bone group mapping available!\n");
				}
			}

			// Get geometry chunk for texture/shader info
			const auto& geometry_chunk_uts0 = using_other_model_format ?
				selected_ffna_model_file_other.geometry_chunk.tex_and_vertex_shader_struct.uts0 :
				selected_ffna_model_file.geometry_chunk.tex_and_vertex_shader_struct.uts0;
			const auto& geometry_chunk_uts1 = using_other_model_format ?
				selected_ffna_model_file_other.geometry_chunk.uts1 :
				selected_ffna_model_file.geometry_chunk.uts1;

			std::vector<int> sort_orders;
			for (int i = 0; i < models.size(); i++)
			{
				AMAT_file amat_file;
				// "Other" format doesn't have AMAT filenames chunk, only standard format does
				if (!using_other_model_format && selected_ffna_model_file.AMAT_filenames_chunk.texture_filenames.size() > 0) {
					int sub_model_index = models[i].unknown;
					if (geometry_chunk_uts0.size() > 0)
					{
						sub_model_index %= geometry_chunk_uts0.size();
					}
					const auto uts1 = geometry_chunk_uts1[sub_model_index % geometry_chunk_uts1.size()];

					const int amat_file_index = ((uts1.some_flags0 >> 8) & 0xFF) % selected_ffna_model_file.AMAT_filenames_chunk.texture_filenames.size();
					const auto amat_filename = selected_ffna_model_file.AMAT_filenames_chunk.texture_filenames[amat_file_index];

					const auto decoded_filename = decode_filename(amat_filename.id0, amat_filename.id1);


					auto mft_entry_it = hash_index.find(decoded_filename);
					if (mft_entry_it != hash_index.end())
					{
						auto file_index = mft_entry_it->second.at(0);
						amat_file = dat_manager->parse_amat_file(file_index);
					}
				}
				Mesh prop_mesh = using_other_model_format ?
					selected_ffna_model_file_other.GetMesh(i, amat_file) :
					selected_ffna_model_file.GetMesh(i, amat_file);
				prop_mesh.center = {
					(models[i].maxX - models[i].minX) / 2.0f, (models[i].maxY - models[i].minY) / 2.0f,
					(models[i].maxZ - models[i].minZ) / 2.0f
				};

				if (using_other_model_format) {
					char debug_msg[512];
					sprintf_s(debug_msg, "draw_dat_browser: model[%d] bounds min=(%.2f,%.2f,%.2f) max=(%.2f,%.2f,%.2f)\n",
						i, models[i].minX, models[i].minY, models[i].minZ,
						models[i].maxX, models[i].maxY, models[i].maxZ);
					LogBB8Debug(debug_msg);
					sprintf_s(debug_msg, "draw_dat_browser: prop_mesh vertices=%zu, indices=%zu\n",
						prop_mesh.vertices.size(), prop_mesh.indices.size());
					LogBB8Debug(debug_msg);
				}

				overallMinX = std::min(overallMinX, models[i].minX);
				overallMinY = std::min(overallMinY, models[i].minY);
				overallMinZ = std::min(overallMinZ, models[i].minZ);

				overallMaxX = std::max(overallMaxX, models[i].maxX);
				overallMaxY = std::max(overallMaxY, models[i].maxY);
				overallMaxZ = std::max(overallMaxZ, models[i].maxZ);

				uint32_t sort_order = amat_file.GRMT_chunk.sort_order;
				sort_orders.push_back(sort_order);

				if ((prop_mesh.indices.size() % 3) == 0) {
					prop_meshes.push_back(prop_mesh);
					g_animationState.originalMeshes.push_back(prop_mesh);
					if (using_other_model_format) {
						LogBB8Debug("draw_dat_browser: Mesh added to prop_meshes\n");
					}
				} else if (using_other_model_format) {
					char debug_msg[256];
					sprintf_s(debug_msg, "draw_dat_browser: Mesh REJECTED (indices %% 3 != 0)\n");
					LogBB8Debug(debug_msg);
				}
			}

			std::vector<size_t> indices(sort_orders.size());
			std::iota(indices.begin(), indices.end(), 0);  // Fill with 0, 1, 2, ...

			std::sort(indices.begin(), indices.end(),
				[&sort_orders](size_t i1, size_t i2) { return sort_orders[i1] < sort_orders[i2]; });

			// Create new vectors with sorted elements
			std::vector<Mesh> sorted_prop_meshes(prop_meshes.size());
			std::vector<int> sorted_sort_orders(sort_orders.size());

			for (size_t i = 0; i < indices.size(); ++i) {
				sorted_prop_meshes[i] = prop_meshes[indices[i]];
				sorted_sort_orders[i] = sort_orders[indices[i]];
			}

			// If you want, you can now swap the sorted vectors with the original ones
			prop_meshes.swap(sorted_prop_meshes);
			sort_orders.swap(sorted_sort_orders);

			// Load textures
			std::vector<int> texture_ids;
			std::vector<DatTexture> model_dat_textures;
			std::vector<std::vector<int>> per_mesh_tex_ids(prop_meshes.size());

			// Check if we should load inline textures (for "other" format) or file references
			bool textures_available = using_other_model_format ?
				(selected_ffna_model_file_other.has_inline_textures || selected_ffna_model_file_other.textures_parsed_correctly) :
				selected_ffna_model_file.textures_parsed_correctly;

			if (using_other_model_format)
			{
				char debug_msg[256];
				sprintf_s(debug_msg, "draw_dat_browser: textures_available=%d, has_inline=%d, textures_parsed=%d\n",
					textures_available ? 1 : 0,
					selected_ffna_model_file_other.has_inline_textures ? 1 : 0,
					selected_ffna_model_file_other.textures_parsed_correctly ? 1 : 0);
				LogBB8Debug(debug_msg);
				sprintf_s(debug_msg, "draw_dat_browser: texture_filenames.size()=%zu\n",
					selected_ffna_model_file_other.texture_filenames_chunk.texture_filenames.size());
				LogBB8Debug(debug_msg);
			}

			if (textures_available)
			{
				// Clear texture display vectors
				inline_texture_displays.clear();
				model_texture_displays.clear();

				// Handle inline textures for "other" format (these are inventory icons)
				if (using_other_model_format && selected_ffna_model_file_other.has_inline_textures)
				{

					auto inline_textures = selected_ffna_model_file_other.GetAllInlineTextures();
					const auto& texture_filenames = selected_ffna_model_file_other.texture_filenames_chunk.texture_filenames;

					char debug_msg[256];
					sprintf_s(debug_msg, "draw_dat_browser: Loading %zu INLINE textures for model (texture_filenames=%zu)\n",
						inline_textures.size(), texture_filenames.size());
					LogBB8Debug(debug_msg);

					// Log raw chunk data to understand the structure
					const auto& chunk = selected_ffna_model_file_other.texture_filenames_chunk;
					sprintf_s(debug_msg, "  TEXTURE_CHUNK: chunk_id=0x%X, chunk_size=%u, num_filenames=%u, actual=%u\n",
						chunk.chunk_id, chunk.chunk_size, chunk.num_texture_filenames, chunk.actual_num_texture_filenames);
					LogBB8Debug(debug_msg);

					// Dump raw chunk data (first 64 bytes)
					if (!chunk.chunk_data.empty())
					{
						sprintf_s(debug_msg, "  RAW CHUNK DATA (first %zu bytes):\n", std::min(chunk.chunk_data.size(), (size_t)64));
						LogBB8Debug(debug_msg);
						for (size_t b = 0; b < std::min(chunk.chunk_data.size(), (size_t)64); b += 16)
						{
							char hex_line[128];
							int pos = sprintf_s(hex_line, "    %04zX: ", b);
							for (size_t j = 0; j < 16 && b + j < chunk.chunk_data.size(); j++)
							{
								pos += sprintf_s(hex_line + pos, sizeof(hex_line) - pos, "%02X ", chunk.chunk_data[b + j]);
							}
							strcat_s(hex_line, "\n");
							LogBB8Debug(hex_line);
						}
					}

					// Log texture filename references (decoded file IDs)
					// TextureFileNameOther has: id0, id1, unknown (6 bytes total)
					for (size_t k = 0; k < texture_filenames.size(); k++)
					{
						auto decoded_filename = decode_filename(texture_filenames[k].id0, texture_filenames[k].id1);
						sprintf_s(debug_msg, "  TEXTURE_FILENAME[%zu]: id0=%u, id1=%u, unk=%u -> decoded=0x%X\n",
							k, texture_filenames[k].id0, texture_filenames[k].id1,
							texture_filenames[k].unknown, decoded_filename);
						LogBB8Debug(debug_msg);
					}

					for (size_t j = 0; j < inline_textures.size(); j++)
					{
						auto& dat_texture = inline_textures[j];

						// Log inline texture info
						std::string format_str = "unknown";
						if (j < selected_ffna_model_file_other.inline_textures.size())
						{
							format_str = selected_ffna_model_file_other.inline_textures[j].GetFormatString();
						}
						sprintf_s(debug_msg, "  INLINE_TEXTURE[%zu]: format=%s, size=%dx%d, rgba_bytes=%zu\n",
							j, format_str.c_str(), dat_texture.width, dat_texture.height, dat_texture.rgba_data.size());
						LogBB8Debug(debug_msg);

						if (dat_texture.width > 0 && dat_texture.height > 0)
						{
							int texture_id = -1;
							// Use a unique hash based on entry hash and texture index
							int texture_hash = entry->Hash * 1000 + static_cast<int>(j);
							auto HR = map_renderer->GetTextureManager()->CreateTextureFromRGBA(
								dat_texture.width, dat_texture.height,
								dat_texture.rgba_data.data(), &texture_id, texture_hash);

							// Note: We still track the texture type but don't add to model_dat_textures
							// Inline textures are inventory icons, not model textures
							model_texture_types.insert({ texture_id, dat_texture.texture_type });

							if (texture_id >= 0)
							{
								// Don't add inline textures to texture_ids - they're inventory icons, not for model rendering
								// texture_ids.push_back(texture_id);

								sprintf_s(debug_msg, "  Created inline_texture[%zu] (inventory icon), internal_id=%d\n",
									j, texture_id);
								LogBB8Debug(debug_msg);

								// Add to inline texture displays for the texture panel only
								InlineTextureDisplay tex_display;
								tex_display.texture_id = texture_id;
								tex_display.width = dat_texture.width;
								tex_display.height = dat_texture.height;
								tex_display.index = static_cast<int>(j);
								tex_display.format = format_str;
								inline_texture_displays.push_back(tex_display);
							}
						}
					}
				}

				// Helper lambda to load a texture by decoded filename and optionally add to model_texture_displays
				auto load_texture_by_hash = [&](size_t j, int decoded_filename, bool add_to_display) {
						int texture_id = map_renderer->GetTextureManager()->GetTextureIdByHash(decoded_filename);

						auto mft_entry_it = hash_index.find(decoded_filename);
						if (mft_entry_it != hash_index.end())
						{
							auto file_index = mft_entry_it->second.at(0);
							const auto* tex_entry = &MFT[file_index];

							if (!tex_entry)
								return;

							DatTexture dat_texture;
							if (tex_entry->type == DDS)
							{
								const auto ddsData = dat_manager->parse_dds_file(file_index);
								size_t ddsDataSize = ddsData.size();
								const auto hr = map_renderer->GetTextureManager()->
									CreateTextureFromDDSInMemory(ddsData.data(), ddsDataSize, &texture_id, &dat_texture.width,
										&dat_texture.height, dat_texture.rgba_data, tex_entry->Hash);
								model_texture_types.insert({ texture_id, DDSt });
								dat_texture.texture_type = DDSt;
							}
							else
							{
								dat_texture = dat_manager->parse_ffna_texture_file(file_index);
								// Create texture if it wasn't cached.
								if (texture_id < 0)
								{
									auto HR = map_renderer->GetTextureManager()->CreateTextureFromRGBA(dat_texture.width,
										dat_texture.height, dat_texture.rgba_data.data(), &texture_id,
										decoded_filename);
								}

								model_texture_types.insert({ texture_id, dat_texture.texture_type });
							}

							model_dat_textures.push_back(dat_texture);

							if (texture_id >= 0) { texture_ids.push_back(texture_id); }

							if (using_other_model_format)
							{
								char debug_msg[256];
								sprintf_s(debug_msg, "draw_dat_browser: LOADED texture[%zu] id=%d, size=%dx%d\n",
									j, texture_id, dat_texture.width, dat_texture.height);
								LogBB8Debug(debug_msg);
							}

							// Add to model texture displays if requested
							if (add_to_display && texture_id >= 0)
							{
								ModelTextureDisplay tex_display;
								tex_display.texture_id = texture_id;
								tex_display.width = dat_texture.width;
								tex_display.height = dat_texture.height;
								tex_display.file_hash = decoded_filename;
								tex_display.index = static_cast<int>(j);
								model_texture_displays.push_back(tex_display);
							}
						}
						else if (using_other_model_format)
						{
							char debug_msg[256];
							sprintf_s(debug_msg, "draw_dat_browser: texture[%zu] hash=0x%X NOT FOUND in hash_index\n", j, decoded_filename);
							LogBB8Debug(debug_msg);
						}
					};

				// Load texture filename references (model textures)
				if (using_other_model_format)
				{
					LogBB8Debug("draw_dat_browser: Loading textures from texture_filenames (other format)\n");
					const auto& texture_filenames = selected_ffna_model_file_other.texture_filenames_chunk.texture_filenames;

					// Pre-size arrays to preserve index correspondence with texture_filenames
					// This is critical because GetMesh returns tex_indices that refer to positions in texture_filenames
					texture_ids.resize(texture_filenames.size(), -1);
					model_dat_textures.resize(texture_filenames.size());

					for (size_t j = 0; j < texture_filenames.size(); j++)
					{
						auto decoded_filename = decode_filename(texture_filenames[j].id0, texture_filenames[j].id1);
						char debug_msg[256];
						sprintf_s(debug_msg, "draw_dat_browser: texture[%zu] id0=%u, id1=%u, decoded_hash=0x%X\n",
							j, texture_filenames[j].id0, texture_filenames[j].id1, decoded_filename);
						LogBB8Debug(debug_msg);

						// Load texture data
						auto mft_entry_it = hash_index.find(decoded_filename);
						if (mft_entry_it != hash_index.end())
						{
							auto file_index = mft_entry_it->second.at(0);
							const auto* tex_entry = &MFT[file_index];

							if (tex_entry)
							{
								DatTexture dat_texture;
								int texture_id = -1;

								if (tex_entry->type == DDS)
								{
									const auto ddsData = dat_manager->parse_dds_file(file_index);
									size_t ddsDataSize = ddsData.size();
									map_renderer->GetTextureManager()->
										CreateTextureFromDDSInMemory(ddsData.data(), ddsDataSize, &texture_id, &dat_texture.width,
											&dat_texture.height, dat_texture.rgba_data, tex_entry->Hash);
									dat_texture.texture_type = DDSt;
								}
								else
								{
									dat_texture = dat_manager->parse_ffna_texture_file(file_index);
									// Create GPU texture for non-DDS textures
									if (dat_texture.width > 0 && dat_texture.height > 0 && !dat_texture.rgba_data.empty())
									{
										map_renderer->GetTextureManager()->CreateTextureFromRGBA(
											dat_texture.width, dat_texture.height,
											dat_texture.rgba_data.data(), &texture_id, decoded_filename);
									}
								}

								// Store at the correct index to preserve correspondence with texture_filenames
								texture_ids[j] = texture_id;
								model_dat_textures[j] = dat_texture;
								if (texture_id >= 0)
								{
									model_texture_types.insert({ texture_id, dat_texture.texture_type });
								}

								sprintf_s(debug_msg, "draw_dat_browser: texture[%zu] loaded, texture_id=%d, size=%dx%d\n",
									j, texture_id, dat_texture.width, dat_texture.height);
								LogBB8Debug(debug_msg);

								// Add to model texture displays
								ModelTextureDisplay tex_display;
								tex_display.texture_id = texture_id;
								tex_display.width = dat_texture.width;
								tex_display.height = dat_texture.height;
								tex_display.file_hash = decoded_filename;
								tex_display.index = static_cast<int>(j);
								model_texture_displays.push_back(tex_display);
							}
						}
						else
						{
							sprintf_s(debug_msg, "draw_dat_browser: texture[%zu] hash=0x%X NOT FOUND\n", j, decoded_filename);
							LogBB8Debug(debug_msg);
						}
					}

					// Log final texture_ids for debugging
					char debug_msg[512];
					sprintf_s(debug_msg, "draw_dat_browser: Final texture_ids (count=%zu):", texture_ids.size());
					LogBB8Debug(debug_msg);
					for (size_t t = 0; t < texture_ids.size(); t++)
					{
						sprintf_s(debug_msg, "  texture_ids[%zu] = %d\n", t, texture_ids[t]);
						LogBB8Debug(debug_msg);
					}
				}
				else
				{
					const auto& texture_filenames = selected_ffna_model_file.texture_filenames_chunk.texture_filenames;
					for (size_t j = 0; j < texture_filenames.size(); j++)
					{
						auto decoded_filename = decode_filename(texture_filenames[j].id0, texture_filenames[j].id1);
						load_texture_by_hash(j, decoded_filename, false);  // Don't add to model_texture_displays for standard models
					}
				}

				selected_dat_texture.dat_texture =
					map_renderer->GetTextureManager()->BuildTextureAtlas(model_dat_textures, -1, -1);

				if (selected_dat_texture.dat_texture.width > 0 && selected_dat_texture.dat_texture.height > 0)
				{
					map_renderer->GetTextureManager()->CreateTextureFromRGBA(
						selected_dat_texture.dat_texture.width,
						selected_dat_texture.dat_texture.height,
						selected_dat_texture.dat_texture.rgba_data.
						data(), &selected_dat_texture.texture_id,
						entry->Hash);

					selected_dat_texture.file_id = entry->Hash;
				}

				// If the number of textures is 0 we add a placeholder texture to make the object visible
				if (texture_ids.size() == 0)
				{
					int texture_tile_size = 8;
					int texture_width = texture_tile_size * 2;
					int texture_height = texture_tile_size * 2;
					CheckerboardTexture::ColorChoice color_choice0 = CheckerboardTexture::ColorChoice::Silver;

					CheckerboardTexture checkerboard_texture_0(texture_width, texture_height, texture_tile_size, color_choice0, color_choice0);

					const auto checkered_tex_id_0 =
						map_renderer->GetTextureManager()->AddTexture((void*)checkerboard_texture_0.getData().data(), texture_width,
							texture_height, DXGI_FORMAT_R8G8B8A8_UNORM, 3214234 + (int)color_choice0 * 20 + (int)color_choice0);

					model_texture_types.insert({ checkered_tex_id_0, BC1 });


					texture_ids.push_back(checkered_tex_id_0);


					for (int i = 0; i < prop_meshes.size(); i++)
					{
						// set blend flag to 0 to make it opaque
						for (int j = 0; j < prop_meshes[i].blend_flags.size(); j++)
						{
							prop_meshes[i].blend_flags[j] = 0;
						}
					}
				}

				// The number of textures might exceed 8 for a model since each submodel might use up to 8 separate textures.
				// So for each submodel's Mesh we must make sure that the uv_indices[i] < 8 and tex_indices[i] < 8.
				// We also need to keep tex_indices, uv_coord_indices, and blend_flags arrays in sync.
				for (int i = 0; i < prop_meshes.size(); i++)
				{
					std::vector<uint8_t> mesh_tex_indices;
					std::vector<uint8_t> mesh_uv_coord_indices;
					std::vector<uint8_t> mesh_blend_flags;

					for (int j = 0; j < prop_meshes[i].tex_indices.size(); j++)
					{
						int tex_index = prop_meshes[i].tex_indices[j];
						// Check if index is valid and texture was successfully loaded (texture_ids[tex_index] >= 0)
						if (tex_index >= 0 && tex_index < texture_ids.size() && texture_ids[tex_index] >= 0)
						{
							per_mesh_tex_ids[i].push_back(texture_ids[tex_index]);
							mesh_tex_indices.push_back(static_cast<uint8_t>(per_mesh_tex_ids[i].size() - 1));

							// Keep uv_coord_indices and blend_flags in sync
							if (j < prop_meshes[i].uv_coord_indices.size())
								mesh_uv_coord_indices.push_back(prop_meshes[i].uv_coord_indices[j]);
							if (j < prop_meshes[i].blend_flags.size())
								mesh_blend_flags.push_back(prop_meshes[i].blend_flags[j]);

							if (using_other_model_format)
							{
								char debug_msg[256];
								sprintf_s(debug_msg, "draw_dat_browser: mesh[%d] tex_slot[%zu]: global_idx=%d -> texture_id=%d, blend=%d\n",
									i, per_mesh_tex_ids[i].size() - 1, tex_index, texture_ids[tex_index],
									(j < prop_meshes[i].blend_flags.size()) ? prop_meshes[i].blend_flags[j] : -1);
								LogBB8Debug(debug_msg);
							}
						}
						else if (using_other_model_format)
						{
							char debug_msg[256];
							sprintf_s(debug_msg, "draw_dat_browser: mesh[%d] tex_pair[%d]: SKIPPED (tex_index=%d, valid=%d)\n",
								i, j, tex_index, (tex_index >= 0 && tex_index < texture_ids.size() && texture_ids[tex_index] >= 0) ? 1 : 0);
							LogBB8Debug(debug_msg);
						}
					}

					prop_meshes[i].tex_indices = mesh_tex_indices;
					prop_meshes[i].uv_coord_indices = mesh_uv_coord_indices;
					prop_meshes[i].blend_flags = mesh_blend_flags;
				}
			}

			// Create the PerObjectCB for each submodel
			std::vector<PerObjectCB> per_object_cbs;
			per_object_cbs.resize(prop_meshes.size());
			for (int i = 0; i < per_object_cbs.size(); i++)
			{
				float modelWidth = overallMaxX - overallMinX;
				float modelHeight = overallMaxY - overallMinY;
				float modelDepth = overallMaxZ - overallMinZ;

				float maxDimension = std::max({ modelWidth, modelHeight, modelDepth });

				float boundingBoxSize = 10000.0f;
				float scale = boundingBoxSize / maxDimension;

				float centerX = overallMinX + modelWidth * 0.5f;
				float centerY = overallMinY + modelHeight * 0.5f;
				float centerZ = overallMinZ + modelDepth * 0.5f;

				XMMATRIX scaling_matrix = XMMatrixScaling(scale, scale, scale);
				XMMATRIX translation_matrix =
					XMMatrixTranslation(-centerX * scale, (-centerY + modelHeight / 2) * scale, -centerZ * scale);
				XMMATRIX world_matrix = scaling_matrix * translation_matrix;

				XMStoreFloat4x4(&per_object_cbs[i].world, world_matrix);

				auto& prop_mesh = prop_meshes[i];
				if (prop_mesh.uv_coord_indices.size() != prop_mesh.tex_indices.size() ||
					prop_mesh.uv_coord_indices.size() >= MAX_NUM_TEX_INDICES)
				{
					if (using_other_model_format)
						selected_ffna_model_file_other.textures_parsed_correctly = false;
					else
						selected_ffna_model_file.textures_parsed_correctly = false;
					continue;
				}

				if (textures_available)
				{
					per_object_cbs[i].num_uv_texture_pairs = prop_mesh.uv_coord_indices.size();

					if (using_other_model_format)
					{
						char debug_msg[256];
						sprintf_s(debug_msg, "draw_dat_browser: mesh[%d] num_tex_pairs=%zu\n",
							i, prop_mesh.uv_coord_indices.size());
						LogBB8Debug(debug_msg);
					}

					for (int j = 0; j < prop_mesh.uv_coord_indices.size(); j++)
					{
						int index0 = j / 4;
						int index1 = j % 4;

						per_object_cbs[i].uv_indices[index0][index1] =
							static_cast<uint32_t>(prop_mesh.uv_coord_indices[j]);
						per_object_cbs[i].texture_indices[index0][index1] =
							static_cast<uint32_t>(prop_mesh.tex_indices[j]);
						per_object_cbs[i].blend_flags[index0][index1] = static_cast<uint32_t>(prop_mesh.blend_flags[j]);
						per_object_cbs[i].texture_types[index0][index1] = static_cast<uint32_t>(model_texture_types.at(per_mesh_tex_ids[i][j]));

						if (using_other_model_format)
						{
							char debug_msg[256];
							sprintf_s(debug_msg, "  CB[%d] slot[%d]: uv_idx=%u, tex_idx=%u, blend=%u, tex_type=%u, tex_id=%d\n",
								i, j, per_object_cbs[i].uv_indices[index0][index1],
								per_object_cbs[i].texture_indices[index0][index1],
								per_object_cbs[i].blend_flags[index0][index1],
								per_object_cbs[i].texture_types[index0][index1],
								per_mesh_tex_ids[i][j]);
							LogBB8Debug(debug_msg);
						}
					}
				}
			}

			auto pixel_shader_type = PixelShaderType::OldModel;
			const auto& unknown_tex_stuff1 = using_other_model_format ?
				selected_ffna_model_file_other.geometry_chunk.unknown_tex_stuff1 :
				selected_ffna_model_file.geometry_chunk.unknown_tex_stuff1;
			if (unknown_tex_stuff1.size() > 0)
			{
				pixel_shader_type = PixelShaderType::NewModel;
			}

			auto mesh_ids = map_renderer->AddProp(prop_meshes, per_object_cbs, index, pixel_shader_type);

			// Store mesh IDs for submesh visibility control
			g_animationState.meshIds = mesh_ids;

			// Store per-object CB data for skinned rendering
			g_animationState.perMeshPerObjectCB = per_object_cbs;

			// Store the mesh scale factor (skeleton needs to match mesh scale)
			float modelWidth = overallMaxX - overallMinX;
			float modelHeight = overallMaxY - overallMinY;
			float modelDepth = overallMaxZ - overallMinZ;
			float maxDimension = std::max({ modelWidth, modelHeight, modelDepth });
			g_animationState.meshScale = (maxDimension > 0.001f) ? (10000.0f / maxDimension) : 1.0f;

			// Store texture IDs for skinned rendering
			g_animationState.perMeshTextureIds = per_mesh_tex_ids;

			if (textures_available)
			{
				for (int i = 0; i < mesh_ids.size(); i++)
				{
					int mesh_id = mesh_ids[i];
					auto& mesh_texture_ids = per_mesh_tex_ids[i];

					if (using_other_model_format)
					{
						char debug_msg[256];
						sprintf_s(debug_msg, "draw_dat_browser: SetTexturesForMesh mesh_id=%d, num_textures=%zu\n",
							mesh_id, mesh_texture_ids.size());
						LogBB8Debug(debug_msg);
						for (size_t t = 0; t < mesh_texture_ids.size(); t++)
						{
							sprintf_s(debug_msg, "  texture[%zu] = internal_id %d\n", t, mesh_texture_ids[t]);
							LogBB8Debug(debug_msg);
						}
					}

					map_renderer->GetMeshManager()->SetTexturesForMesh(
						mesh_id,
						map_renderer->GetTextureManager()->
						GetTextures(mesh_texture_ids), 3);
				}
			}
			success = true;

			// Auto-activate model viewer when loading a model
			ActivateModelViewer(map_renderer);
			g_modelViewerState.modelFileId = entry->Hash;
			g_modelViewerState.modelMftIndex = index;
			g_modelViewerState.modelDatManager = dat_manager;
			GuiGlobalConstants::is_model_viewer_panel_open = true;

			// After the mesh is ready and visible, start animation discovery in background.
			AutoLoadAnimationFromStoredManagers();
		}

		break;
	case FFNA_Type3:
	{
		if (selected_map_file_index == index)
		{
			break;
		}

		// Switching to map mode must clear all model-viewer/animation state first.
		CancelAnimationSearch();
		if (g_modelViewerState.isActive)
		{
			DeactivateModelViewer(map_renderer);
		}
		else
		{
			map_renderer->ClearCameraOverride();
		}
		g_animationState.Reset();
		g_modelViewerState.Reset();
		map_renderer->ClearSceneForModeSwitch();
		map_renderer->SetShouldRenderShadowsForModels(true);



		selected_map_file_index = index;

		object_id_to_prop_index.clear();
		object_id_to_submodel_index.clear();
		selected_map_files.clear();
		selected_ffna_map_file = dat_manager->parse_ffna_map_file(index);

		if (selected_ffna_map_file.terrain_chunk.terrain_heightmap.size() > 0 &&
			selected_ffna_map_file.terrain_chunk.terrain_heightmap.size() ==
			selected_ffna_map_file.terrain_chunk.terrain_x_dims *
			selected_ffna_map_file.terrain_chunk.terrain_y_dims)
		{
			// Clear up some GPU memory (especially important for GPUs with little VRAM)
			map_renderer->GetTextureManager()->Clear();


			const auto& environment_info_chunk = selected_ffna_map_file.environment_info_chunk;
			const EnvSubChunk8* env8 = environment_info_chunk.env_sub_chunk8.empty()
				? nullptr
				: &environment_info_chunk.env_sub_chunk8[0];

			// Sky shader params (defaults). We'll override from env chunks when available.
			PerSkyCB sky_cb = map_renderer->GetPerSkyCB();

			// Brightness/saturation are in the sky settings (EnvSubChunk1).
			{
				float brightness = 1.0f;
				float saturation = 1.0f;
				float bias_add = 0.0f;

				if (!environment_info_chunk.env_sub_chunk1.empty()) {
					const size_t sky_settings_idx =
						(env8 && env8->sky_settings_index < environment_info_chunk.env_sub_chunk1.size())
						? env8->sky_settings_index
						: 0u;
					const auto& sub1 = environment_info_chunk.env_sub_chunk1[sky_settings_idx];

					// Interpret bytes as centered at 128 (neutral).
					brightness = std::clamp(sub1.sky_brightness_maybe / 128.0f, 0.0f, 2.0f);
					saturation = std::clamp(sub1.sky_saturaion_maybe / 128.0f, 0.0f, 2.0f);
				}

				if (env8) {
					bias_add = (static_cast<int>(env8->sky_brightness_bias) - 128) / 128.0f;
					bias_add = std::clamp(bias_add * 0.15f, -0.25f, 0.25f);
				}

				sky_cb.color_params = DirectX::XMFLOAT4(brightness, saturation, bias_add, 0.0f);
			}

			// Set ambient and diffuse light
			if (!environment_info_chunk.env_sub_chunk3.empty()) {
				const size_t lighting_idx =
					(env8 && env8->lighting_settings_index < environment_info_chunk.env_sub_chunk3.size())
					? env8->lighting_settings_index
					: 0u;
				const auto& sub3 = environment_info_chunk.env_sub_chunk3[lighting_idx];

				float light_div_factor = 2.0f;

				float ambient_intensity = sub3.ambient_intensity / 255.0f;
				float diffuse_intensity = sub3.sun_intensity / 255.0f;

				DirectionalLight directional_light = map_renderer->GetDirectionalLight();
				directional_light.ambient.x = sub3.ambient_red / (255.0f * light_div_factor);
				directional_light.ambient.y = sub3.ambient_green / (255.0f * light_div_factor);
				directional_light.ambient.z = sub3.ambient_blue / (255.0f * light_div_factor);

				directional_light.diffuse.x = sub3.sun_red / (255.0f * light_div_factor);
				directional_light.diffuse.y = sub3.sun_green / (255.0f * light_div_factor);
				directional_light.diffuse.z = sub3.sun_blue / (255.0f * light_div_factor);

				auto ambient_hls = RGBAtoHSL(directional_light.ambient);
				auto diffuse_hls = RGBAtoHSL(directional_light.diffuse);

				// Increase lightness
				ambient_hls.z = std::max(ambient_intensity * 0.9, 0.7);
				diffuse_hls.z = std::max(diffuse_intensity * 0.9, 0.5);

				directional_light.ambient = HSLtoRGBA(ambient_hls);
				directional_light.diffuse = HSLtoRGBA(diffuse_hls);

				map_renderer->SetDirectionalLight(directional_light);
			}

			uint16_t sky_background_filename_index = 0xFFFF;
			uint16_t sky_clouds_texture_filename_index0 = 0xFFFF;
			uint16_t sky_clouds_texture_filename_index1 = 0xFFFF;
			uint16_t sky_sun_texture_filename_index = 0xFFFF;
			uint16_t cloud_texture_filename_index = 0;
			uint16_t water_color_texture_filename_index = 0xFFFF;
			uint16_t water_distortion_texture_filename_index = 0xFFFF;

			const uint16_t selected_sky_texture_settings_index =
				env8 ? env8->sky_texture_settings_index : 0u;
			const uint16_t selected_water_settings_index =
				env8 ? env8->water_settings_index : 0u;
			const uint16_t selected_wind_settings_index =
				env8 ? env8->wind_settings_index : 0u;

			if (!environment_info_chunk.env_sub_chunk5.empty()) {
				const size_t sky_idx =
					(selected_sky_texture_settings_index < environment_info_chunk.env_sub_chunk5.size())
					? selected_sky_texture_settings_index
					: 0u;
				const auto& sub5 = environment_info_chunk.env_sub_chunk5[sky_idx];
				sky_background_filename_index = sub5.sky_background_texture_index;
				sky_clouds_texture_filename_index0 = sub5.sky_clouds_texture_index0;
				sky_clouds_texture_filename_index1 = sub5.sky_clouds_texture_index1;
				sky_sun_texture_filename_index = sub5.sky_sun_texture_index;

				// Decode sky layer params.
				//
				// D3D9 dumps show the moving cloud layer does a UV translate only in the VS:
				//   oT0.x = u + c4.z
				//   oT0.y = v + c5.z
				// So treat unknown1/unknown2 as scroll U/V for ONE layer (not two independent layers).
				//
				// unknown0 appears to influence tiling; if we use a non-integer repeat with WRAP, we get a hard seam at u=0/1.
				// Round to an integer repeat count to preserve wrapping.
				const float uv_scale = std::max(1.0f, std::round(sub5.unknown0 / 32.0f));
				// These int16 values appear fixed-point; /4096 was ~1-2 orders too fast in practice.
				const float kCloudScrollDenom = 16777216.0f; // 4096 * 4096 (empirical)
				const float scroll_u = static_cast<float>(sub5.unknown1) / kCloudScrollDenom;
				const float scroll_v = static_cast<float>(sub5.unknown2) / kCloudScrollDenom;
				const float sun_scale = sub5.unknown3 / 255.0f;
				const float sun_disk_radius = (0.01f + sun_scale * 0.05f) * 3.0f;

				sky_cb.cloud0_params = DirectX::XMFLOAT4(uv_scale, scroll_u, scroll_v, 1.0f);
				// Disable the second moving layer for now; we haven't observed a second translated cloud pass in the dumps.
				sky_cb.cloud1_params = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
				sky_cb.sun_params.x = sun_disk_radius;
			}
			else if (!environment_info_chunk.env_sub_chunk5_other.empty()) {
				const size_t sky_idx =
					(selected_sky_texture_settings_index < environment_info_chunk.env_sub_chunk5_other.size())
					? selected_sky_texture_settings_index
					: 0u;
				const auto& sub5 = environment_info_chunk.env_sub_chunk5_other[sky_idx];
				sky_background_filename_index = sub5.sky_background_texture_index;
				sky_clouds_texture_filename_index0 = sub5.sky_clouds_texture_index0;
				sky_clouds_texture_filename_index1 = sub5.sky_clouds_texture_index1;
				sky_sun_texture_filename_index = sub5.sky_sun_texture_index;

				// Best-effort decode of the "other" layout (unknown[7]) as (scale, int16, int16, sunSize, ...).
				uint8_t scale_byte = 0;
				int16_t scroll0_raw = 0;
				int16_t scroll1_raw = 0;
				uint8_t sun_byte = 0;
				std::memcpy(&scale_byte, &sub5.unknown[0], sizeof(scale_byte));
				std::memcpy(&scroll0_raw, &sub5.unknown[1], sizeof(scroll0_raw));
				std::memcpy(&scroll1_raw, &sub5.unknown[3], sizeof(scroll1_raw));
				std::memcpy(&sun_byte, &sub5.unknown[5], sizeof(sun_byte));

				const float uv_scale = std::max(1.0f, std::round(scale_byte / 32.0f));
				const float kCloudScrollDenom = 16777216.0f; // 4096 * 4096 (empirical)
				const float scroll_u = static_cast<float>(scroll0_raw) / kCloudScrollDenom;
				const float scroll_v = static_cast<float>(scroll1_raw) / kCloudScrollDenom;
				const float sun_scale = sun_byte / 255.0f;
				const float sun_disk_radius = (0.01f + sun_scale * 0.05f) * 3.0f;

				sky_cb.cloud0_params = DirectX::XMFLOAT4(uv_scale, scroll_u, scroll_v, 1.0f);
				sky_cb.cloud1_params = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
				sky_cb.sun_params.x = sun_disk_radius;
			}

			map_renderer->SetPerSkyCB(sky_cb);

			if (!environment_info_chunk.env_sub_chunk6.empty()) {
				const size_t water_idx =
					(selected_water_settings_index < environment_info_chunk.env_sub_chunk6.size())
					? selected_water_settings_index
					: 0u;
				const auto& selected_water = environment_info_chunk.env_sub_chunk6[water_idx];
				water_color_texture_filename_index = selected_water.water_color_texture_index;
				water_distortion_texture_filename_index = selected_water.water_distortion_texture_index;
			}

			std::vector<uint16_t> sky_texture_filename_indices{
				sky_background_filename_index,
				sky_clouds_texture_filename_index0,
				sky_clouds_texture_filename_index1,
				sky_sun_texture_filename_index
			};

			std::vector<ID3D11ShaderResourceView*> sky_textures(sky_texture_filename_indices.size(), nullptr);
			const auto& environment_info_filenames_chunk = selected_ffna_map_file.environment_info_filenames_chunk;
			for (size_t i = 0; i < sky_texture_filename_indices.size(); i++) {
				const auto filename_index = sky_texture_filename_indices[i];
				if (filename_index >= environment_info_filenames_chunk.filenames.size()) {
					continue;
				}

				const auto& filename = environment_info_filenames_chunk.filenames[filename_index];
				auto decoded_filename = decode_filename(filename.filename.id0, filename.filename.id1);

				auto mft_entry_it = hash_index.find(decoded_filename);
				if (mft_entry_it != hash_index.end())
				{
					auto type = dat_manager->get_MFT()[mft_entry_it->second.at(0)].type;
					const DatTexture dat_texture = dat_manager->parse_ffna_texture_file(mft_entry_it->second.at(0));
					int texture_id = -1;
					if (dat_texture.width > 0 && dat_texture.height > 0) {

						auto HR = map_renderer->GetTextureManager()->CreateTextureFromRGBA(
							dat_texture.width, dat_texture.height, dat_texture.rgba_data.data(),
							&texture_id, decoded_filename);
						if (SUCCEEDED(HR) && texture_id >= 0) {
							sky_textures[i] = map_renderer->GetTextureManager()->GetTexture(texture_id);
						}
					}
				}
			}

			if (sky_textures.size() > 0) {
				const int sky_mesh_id = map_renderer->GetMeshManager()->AddGwSkyCylinder(67723.75f / 2.0f, 33941.0f);
				map_renderer->SetSkyMeshId(sky_mesh_id);
				map_renderer->GetMeshManager()->SetMeshShouldCull(sky_mesh_id, false);
				map_renderer->SetMeshShouldRender(sky_mesh_id, false); // we will manually render it first before any other meshes.

				const auto& map_bounds = selected_ffna_map_file.map_info_chunk.map_bounds;

				const float map_center_x = (map_bounds.map_min_x + map_bounds.map_max_x) / 2.0f;
				const float map_center_z = (map_bounds.map_min_z + map_bounds.map_max_z) / 2.0f;

				DirectX::XMFLOAT4X4 sky_world_matrix;
				DirectX::XMStoreFloat4x4(&sky_world_matrix, DirectX::XMMatrixTranslation(map_center_x, map_renderer->GetSkyHeight(), map_center_z));
				PerObjectCB sky_per_object_data;
				sky_per_object_data.world = sky_world_matrix;
				for (int i = 0; i < sky_textures.size(); i++) {
					sky_per_object_data.texture_indices[i / 4][i % 4] = 0;
					sky_per_object_data.texture_types[i / 4][i % 4] = sky_textures[i] == nullptr ? 0xFF : 0;
				}

				sky_per_object_data.num_uv_texture_pairs = sky_textures.size();
				map_renderer->GetMeshManager()->UpdateMeshPerObjectData(sky_mesh_id, sky_per_object_data);

				map_renderer->GetMeshManager()->SetTexturesForMesh(sky_mesh_id, sky_textures, 3);
			}

			// Add cloud mesh and texture
			std::vector<uint16_t> cloud_texture_filename_indices{ cloud_texture_filename_index };

			std::vector<ID3D11ShaderResourceView*> cloud_textures(cloud_texture_filename_indices.size(), nullptr);
			for (size_t i = 0; i < cloud_texture_filename_indices.size(); i++) {
				const auto filename_index = cloud_texture_filename_indices[i];
				if (filename_index >= environment_info_filenames_chunk.filenames.size()) {
					continue;
				}

				const auto& filename = environment_info_filenames_chunk.filenames[filename_index];
				auto decoded_filename = decode_filename(filename.filename.id0, filename.filename.id1);

				auto mft_entry_it = hash_index.find(decoded_filename);
				if (mft_entry_it != hash_index.end())
				{
					auto type = dat_manager->get_MFT()[mft_entry_it->second.at(0)].type;
					const DatTexture dat_texture = dat_manager->parse_ffna_texture_file(mft_entry_it->second.at(0));
					int texture_id = -1;
					if (dat_texture.width == 512 && dat_texture.height == 512) {

						auto HR = map_renderer->GetTextureManager()->CreateTextureFromRGBA(
							dat_texture.width, dat_texture.height, dat_texture.rgba_data.data(),
							&texture_id, decoded_filename);
						if (SUCCEEDED(HR) && texture_id >= 0) {
							cloud_textures[i] = map_renderer->GetTextureManager()->GetTexture(texture_id);
						}
					}
				}
			}

			std::vector<uint16_t> water_texture_filename_indices{ water_color_texture_filename_index, water_distortion_texture_filename_index };

			std::vector<ID3D11ShaderResourceView*> water_textures(water_texture_filename_indices.size(), nullptr);
			for (size_t i = 0; i < water_texture_filename_indices.size(); i++) {
				const auto filename_index = water_texture_filename_indices[i];
				if (filename_index >= environment_info_filenames_chunk.filenames.size()) {
					continue;
				}

				const auto& filename = environment_info_filenames_chunk.filenames[filename_index];
				auto decoded_filename = decode_filename(filename.filename.id0, filename.filename.id1);

				auto mft_entry_it = hash_index.find(decoded_filename);
				if (mft_entry_it != hash_index.end())
				{
					auto type = dat_manager->get_MFT()[mft_entry_it->second.at(0)].type;
					DatTexture dat_texture;
					int texture_id = -1;
					if (type == DDS) {
						const auto ddsData = dat_manager->parse_dds_file(mft_entry_it->second.at(0));
						size_t ddsDataSize = ddsData.size();
						const auto HR = map_renderer->GetTextureManager()->
							CreateTextureFromDDSInMemory(ddsData.data(), ddsDataSize, &texture_id, &dat_texture.width,
								&dat_texture.height, dat_texture.rgba_data, decoded_filename);
						if (SUCCEEDED(HR) && texture_id >= 0) {
							water_textures[i] = map_renderer->GetTextureManager()->GetTexture(texture_id);
							dat_texture.texture_type = DDSt;
						}
					}
					else {
						dat_texture = dat_manager->parse_ffna_texture_file(mft_entry_it->second.at(0));

						auto HR = map_renderer->GetTextureManager()->CreateTextureFromRGBA(
							dat_texture.width, dat_texture.height, dat_texture.rgba_data.data(),
							&texture_id, decoded_filename);
						if (SUCCEEDED(HR) && texture_id >= 0) {
							water_textures[i] = map_renderer->GetTextureManager()->GetTexture(texture_id);
						}
					}
				}
			}

			if (water_textures.size() > 0) {
				const int water_mesh_id = map_renderer->GetMeshManager()->AddGwSkyCircle(70000, PixelShaderType::Water);
				map_renderer->SetWaterMeshId(water_mesh_id);
				map_renderer->GetMeshManager()->SetMeshShouldCull(water_mesh_id, false);
				map_renderer->SetMeshShouldRender(water_mesh_id, false); // we will manually render it first before any other meshes.

				const auto& map_bounds = selected_ffna_map_file.map_info_chunk.map_bounds;

				const float map_center_x = (map_bounds.map_min_x + map_bounds.map_max_x) / 2.0f;
				const float map_center_z = (map_bounds.map_min_z + map_bounds.map_max_z) / 2.0f;

				DirectX::XMFLOAT4X4 water_world_matrix;
				DirectX::XMStoreFloat4x4(&water_world_matrix, DirectX::XMMatrixTranslation(map_center_x, 0, map_center_z));
				PerObjectCB water_per_object_data;
				water_per_object_data.world = water_world_matrix;
				for (int i = 0; i < water_textures.size(); i++) {
					water_per_object_data.texture_indices[i / 4][i % 4] = 0;
					water_per_object_data.texture_types[i / 4][i % 4] = water_textures[i] == nullptr ? 0xFF : 0;
				}

				water_per_object_data.num_uv_texture_pairs = water_textures.size();
				map_renderer->GetMeshManager()->UpdateMeshPerObjectData(water_mesh_id, water_per_object_data);

				map_renderer->GetMeshManager()->SetTexturesForMesh(water_mesh_id, water_textures, 0);
				map_renderer->GetMeshManager()->SetTexturesForMesh(
					water_mesh_id, { map_renderer->GetWaterFresnelLUTSRV() }, 3);
			}

			auto& terrain_texture_filenames = selected_ffna_map_file.terrain_texture_filenames.array;
			std::vector<DatTexture> terrain_dat_textures;
			for (int i = 0; i < terrain_texture_filenames.size(); i++)
			{
				auto decoded_filename =
					decode_filename(selected_ffna_map_file.terrain_texture_filenames.array[i].filename.id0,
						selected_ffna_map_file.terrain_texture_filenames.array[i].filename.id1);

				// Jade Quarry, Island of Jade and The Antechamber . Each on them uses a normal map as their first texture.
				if (decoded_filename == 0x25e09 || decoded_filename == 0x00028615 || decoded_filename == 0x46db6)
				{
					continue;
				}

				auto mft_entry_it = hash_index.find(decoded_filename);
				if (mft_entry_it != hash_index.end())
				{
					const DatTexture dat_texture =
						dat_manager->parse_ffna_texture_file(mft_entry_it->second.at(0));
					if (dat_texture.width > 0 && dat_texture.height > 0) {
						terrain_dat_textures.push_back(dat_texture);
					}
				}
			}

			// For displaying the texture atlas. Not used in rendering.
			if (terrain_dat_textures.size() > 0)
			{
				selected_dat_texture.dat_texture =
					map_renderer->GetTextureManager()->BuildTextureAtlas(terrain_dat_textures, -1, -1);

				if (selected_dat_texture.dat_texture.width > 0 && selected_dat_texture.dat_texture.height > 0)
				{
					map_renderer->GetTextureManager()->CreateTextureFromRGBA(
						selected_dat_texture.dat_texture.width,
						selected_dat_texture.dat_texture.height,
						selected_dat_texture.dat_texture.rgba_data.
						data(), &selected_dat_texture.texture_id,
						entry->Hash);

					selected_dat_texture.file_id = entry->Hash;
				}
			}
			else
			{
				selected_dat_texture.texture_id = -1;
			}

			std::vector<void*> raw_data_ptrs;
			for (int i = 0; i < terrain_dat_textures.size(); i++)
			{
				raw_data_ptrs.push_back(terrain_dat_textures[i].rgba_data.data());
			}

			const auto terrain_texture_id = map_renderer->GetTextureManager()->AddTextureArray(raw_data_ptrs,
				terrain_dat_textures[0].width, terrain_dat_textures[0].height, DXGI_FORMAT_B8G8R8A8_UNORM,
				entry->Hash, true);

			auto& terrain_texture_indices =
				selected_ffna_map_file.terrain_chunk.terrain_texture_indices_maybe;

			auto& terrain_shadow_map =
				selected_ffna_map_file.terrain_chunk.terrain_shadow_map;

			// Create terrain
			terrain = std::make_unique<Terrain>(selected_ffna_map_file.terrain_chunk.terrain_x_dims,
				selected_ffna_map_file.terrain_chunk.terrain_y_dims,
				selected_ffna_map_file.terrain_chunk.terrain_heightmap,
				terrain_texture_indices, terrain_shadow_map,
				selected_ffna_map_file.map_info_chunk.map_bounds);
			map_renderer->SetTerrain(terrain.get(), terrain_texture_id);

			if (!environment_info_chunk.env_sub_chunk6.empty()) {
				const size_t water_idx =
					(selected_water_settings_index < environment_info_chunk.env_sub_chunk6.size())
					? selected_water_settings_index
					: 0u;
				const auto& selected_water = environment_info_chunk.env_sub_chunk6[water_idx];

				const EnvSubChunk7* wind_settings = nullptr;
				if (!environment_info_chunk.env_sub_chunk7.empty()) {
					const size_t wind_idx =
						(selected_wind_settings_index < environment_info_chunk.env_sub_chunk7.size())
						? selected_wind_settings_index
						: 0u;
					wind_settings = &environment_info_chunk.env_sub_chunk7[wind_idx];
				}
				map_renderer->UpdateWaterProperties(selected_water, wind_settings);
			}

			if (cloud_textures.size() > 0) {
				const int clouds_mesh_id = map_renderer->GetMeshManager()->AddGwSkyCircle(100000.0f);
				map_renderer->SetCloudsMeshId(clouds_mesh_id);
				map_renderer->GetMeshManager()->SetMeshShouldCull(clouds_mesh_id, true);
				map_renderer->SetMeshShouldRender(clouds_mesh_id, false); // we will manually render it first before any other meshes.

				const auto& map_bounds = terrain->m_bounds;

				const float map_center_x = (map_bounds.map_min_x + map_bounds.map_max_x) / 2.0f;
				const float map_center_z = (map_bounds.map_min_z + map_bounds.map_max_z) / 2.0f;

				DirectX::XMFLOAT4X4 clouds_world_matrix;
				DirectX::XMStoreFloat4x4(&clouds_world_matrix, DirectX::XMMatrixTranslation(map_center_x, map_bounds.map_max_y + 2400, map_center_z));
				PerObjectCB clouds_per_object_data;
				clouds_per_object_data.world = clouds_world_matrix;
				for (int i = 0; i < cloud_textures.size(); i++) {
					clouds_per_object_data.texture_indices[i / 4][i % 4] = 0;
					clouds_per_object_data.texture_types[i / 4][i % 4] = cloud_textures[i] == nullptr ? 0xFF : 0;
				}

				clouds_per_object_data.num_uv_texture_pairs = cloud_textures.size();
				map_renderer->GetMeshManager()->UpdateMeshPerObjectData(clouds_mesh_id, clouds_per_object_data);

				map_renderer->GetMeshManager()->SetTexturesForMesh(clouds_mesh_id, cloud_textures, 3);
			}

			// Set fog and clear color
			if (!environment_info_chunk.env_sub_chunk2.empty()) {
				const size_t fog_idx =
					(env8 && env8->fog_settings_index < environment_info_chunk.env_sub_chunk2.size())
					? env8->fog_settings_index
					: 0u;
				const auto& sub2 = environment_info_chunk.env_sub_chunk2[fog_idx];

				XMFLOAT4 clear_and_fog_color{
					static_cast<float>(sub2.fog_red) / 255.0f,
					static_cast<float>(sub2.fog_green) / 255.0f,
					static_cast<float>(sub2.fog_blue) / 255.0f,
					1.0f
				};


				map_renderer->SetFogStart(std::max((int)sub2.fog_distance_start, 3000));
				map_renderer->SetFogEnd(std::max((float)sub2.fog_distance_end, map_renderer->GetFogStart() + 15000));
				map_renderer->SetFogStartY(-10000);
				map_renderer->SetFogEndY(1500);

				map_renderer->SetClearColor(clear_and_fog_color);
			}

			map_renderer->SetSkyHeight(0);

			success = true;
		}

		// Load models
		std::vector<int> selected_map_file{};
		for (int i = 0; i < selected_ffna_map_file.prop_filenames_chunk.array.size(); i++)
		{
			auto decoded_filename =
				decode_filename(selected_ffna_map_file.prop_filenames_chunk.array[i].filename.id0,
					selected_ffna_map_file.prop_filenames_chunk.array[i].filename.id1);
			auto mft_entry_it = hash_index.find(decoded_filename);
			if (mft_entry_it != hash_index.end())
			{
				auto type = dat_manager->get_MFT()[mft_entry_it->second.at(0)].type;
				if (type == FFNA_Type2)
				{
					selected_map_files.emplace_back(
						dat_manager->parse_ffna_model_file(mft_entry_it->second.at(0)));
				}
			}
		}

		// Load models
		for (int i = 0; i < selected_ffna_map_file.more_filnames_chunk.array.size(); i++)
		{
			auto decoded_filename =
				decode_filename(selected_ffna_map_file.more_filnames_chunk.array[i].filename.id0,
					selected_ffna_map_file.more_filnames_chunk.array[i].filename.id1);
			auto mft_entry_it = hash_index.find(decoded_filename);
			if (mft_entry_it != hash_index.end())
			{
				auto type = dat_manager->get_MFT()[mft_entry_it->second.at(0)].type;
				if (type == FFNA_Type2)
				{
					auto map_model = dat_manager->parse_ffna_model_file(mft_entry_it->second.at(0));
					selected_map_files.emplace_back(map_model);
				}
			}
		}

		for (int i = 0; i < selected_ffna_map_file.props_info_chunk.prop_array.props_info.size(); i++)
		{
			PropInfo prop_info = selected_ffna_map_file.props_info_chunk.prop_array.props_info[i];

			if (prop_info.filename_index < selected_map_files.size())
			{
				if (auto ffna_model_file_ptr =
					std::get_if<FFNA_ModelFile>(&selected_map_files[prop_info.filename_index]))
				{

					std::vector<std::vector<int>> per_mesh_tex_ids;
					// Load geometry
					const auto& geometry_chunk = ffna_model_file_ptr->geometry_chunk;
					prop_meshes.clear();

					std::vector<int> sort_orders;
					for (int j = 0; j < geometry_chunk.models.size(); j++)
					{
						const auto& models = geometry_chunk.models;

						// Get AMAT file if any
						AMAT_file amat_file;
						if (ffna_model_file_ptr->AMAT_filenames_chunk.texture_filenames.size() > 0) {
							int sub_model_index = models[j].unknown;
							if (geometry_chunk.tex_and_vertex_shader_struct.uts0.size() > 0)
							{
								sub_model_index %= geometry_chunk.tex_and_vertex_shader_struct.uts0.size();
							}
							const auto uts1 = geometry_chunk.uts1[sub_model_index % geometry_chunk.uts1.size()];

							const int amat_file_index = ((uts1.some_flags0 >> 8) & 0xFF) % ffna_model_file_ptr->AMAT_filenames_chunk.texture_filenames.size();
							const auto amat_filename = ffna_model_file_ptr->AMAT_filenames_chunk.texture_filenames[amat_file_index];

							const auto decoded_filename = decode_filename(amat_filename.id0, amat_filename.id1);


							auto mft_entry_it = hash_index.find(decoded_filename);
							if (mft_entry_it != hash_index.end())
							{
								auto file_index = mft_entry_it->second.at(0);
								amat_file = dat_manager->parse_amat_file(file_index);
							}
						}

						Mesh prop_mesh = ffna_model_file_ptr->GetMesh(j, amat_file);
						prop_mesh.center = {
							(models[j].maxX - models[j].minX) / 2.0f, (models[j].maxY - models[j].minY) / 2.0f,
							(models[j].maxZ - models[j].minZ) / 2.0f
						};

						uint32_t sort_order = amat_file.GRMT_chunk.sort_order;
						sort_orders.push_back(sort_order);

						if ((prop_mesh.indices.size() % 3) == 0) { prop_meshes.push_back(prop_mesh); }
					}

					std::vector<size_t> indices(sort_orders.size());
					std::iota(indices.begin(), indices.end(), 0);

					std::sort(indices.begin(), indices.end(),
						[&sort_orders](size_t i1, size_t i2) { return sort_orders[i1] < sort_orders[i2]; });

					// Create new vectors with sorted elements
					std::vector<Mesh> sorted_prop_meshes(prop_meshes.size());
					std::vector<int> sorted_sort_orders(sort_orders.size());

					for (size_t j = 0; j < indices.size(); ++j) {
						sorted_prop_meshes[j] = prop_meshes[indices[j]];
						sorted_sort_orders[j] = sort_orders[indices[j]];
					}

					prop_meshes.swap(sorted_prop_meshes);
					sort_orders.swap(sorted_sort_orders);

					if (ffna_model_file_ptr->parsed_correctly)
					{
						// Load textures
						std::vector<int> texture_ids;
						if (ffna_model_file_ptr->textures_parsed_correctly)
						{
							for (int j = 0;
								j < ffna_model_file_ptr->texture_filenames_chunk.texture_filenames.size();
								j++)
							{
								auto texture_filename =
									ffna_model_file_ptr->texture_filenames_chunk.texture_filenames[j];
								auto decoded_filename =
									decode_filename(texture_filename.id0, texture_filename.id1);

								int texture_id =
									map_renderer->GetTextureManager()->GetTextureIdByHash(decoded_filename);
								if (texture_id >= 0)
								{
									texture_ids.push_back(texture_id);
									continue;
								}

								auto mft_entry_it = hash_index.find(decoded_filename);
								if (mft_entry_it != hash_index.end())
								{
									// Get texture from .dat
									auto dat_texture =
										dat_manager->parse_ffna_texture_file(mft_entry_it->second.at(0));

									// Create texture
									auto HR = map_renderer->GetTextureManager()->CreateTextureFromRGBA(
										dat_texture.width, dat_texture.height, dat_texture.rgba_data.data(),
										&texture_id, decoded_filename);

									model_texture_types.insert({ texture_id, dat_texture.texture_type });

									texture_ids.push_back(texture_id);
								}
							}

							// The number of textures might exceed 8 for a model since each submodel might use up to 8 separate textures.
							// So for each submodel's Mesh we must make sure that the uv_indices[i] < 8 and tex_indices[i] < 8.
							per_mesh_tex_ids.resize(prop_meshes.size());
							for (int k = 0; k < prop_meshes.size(); k++)
							{
								std::vector<uint8_t> mesh_tex_indices;
								for (int j = 0; j < prop_meshes[k].tex_indices.size(); j++)
								{
									int tex_index = std::min(prop_meshes[k].tex_indices[j], (uint8_t)(texture_ids.size() - 1));
									if (tex_index < texture_ids.size())
									{
										per_mesh_tex_ids[k].push_back(texture_ids[tex_index]);

										mesh_tex_indices.push_back(j);
									}
								}

								prop_meshes[k].tex_indices = mesh_tex_indices;
							}
						}

						std::vector<PerObjectCB> per_object_cbs;
						per_object_cbs.resize(prop_meshes.size());
						for (int j = 0; j < per_object_cbs.size(); j++)
						{
							XMFLOAT3 translation(prop_info.x, prop_info.y, prop_info.z);

							XMFLOAT3 vec1{ prop_info.f4, -prop_info.f6, prop_info.f5 };
							XMFLOAT3 vec2{ prop_info.sin_angle, -prop_info.f9, prop_info.cos_angle };

							// Load vectors into XMVECTORs
							XMVECTOR v2 = XMLoadFloat3(&vec1);
							XMVECTOR v3 = XMLoadFloat3(&vec2);

							// Compute the third orthogonal vector with cross product
							// Note: This is for left-handed coordinate systems
							XMVECTOR v1 = XMVector3Cross(v3, v2);

							// Ensure all vectors are normalized
							v1 = XMVector3Normalize(v1);
							v2 = XMVector3Normalize(v2);
							v3 = XMVector3Normalize(v3);

							// Fill the rotation matrix
							auto rotation_matrix = XMMATRIX(
								-v1.m128_f32[0], -v1.m128_f32[1], v1.m128_f32[2], 0.0f,
								v2.m128_f32[0], v2.m128_f32[1], v2.m128_f32[2], 0.0f,
								-v3.m128_f32[0], -v3.m128_f32[1], v3.m128_f32[2], 0.0f,
								0.0f, 0.0f, 0.0f, 1.0f
							);

							// Create the scaling and translation matrices
							const auto scale = prop_info.scaling_factor;
							XMMATRIX scaling_matrix = XMMatrixScaling(scale, scale, scale);
							XMMATRIX translation_matrix = XMMatrixTranslationFromVector(XMLoadFloat3(&translation));

							// Compute the final transformation matrix
							XMMATRIX transform_matrix = scaling_matrix * XMMatrixTranspose(rotation_matrix) *
								translation_matrix;

							// Store the transform matrix into the constant buffer
							XMStoreFloat4x4(&per_object_cbs[j].world, transform_matrix);

							auto& prop_mesh = prop_meshes[j];
							if (prop_mesh.uv_coord_indices.size() != prop_mesh.tex_indices.size() ||
								prop_mesh.uv_coord_indices.size() >= MAX_NUM_TEX_INDICES)
							{
								ffna_model_file_ptr->textures_parsed_correctly = false;
								continue;
							}

							if (ffna_model_file_ptr->textures_parsed_correctly)
								per_object_cbs[j].num_uv_texture_pairs = prop_mesh.uv_coord_indices.size();

							for (int k = 0; k < prop_mesh.uv_coord_indices.size(); k++)
							{
								int index0 = k / 4;
								int index1 = k % 4;

								per_object_cbs[j].uv_indices[index0][index1] =
									static_cast<uint32_t>(prop_mesh.uv_coord_indices[k]);
								per_object_cbs[j].texture_indices[index0][index1] =
									static_cast<uint32_t>(prop_mesh.tex_indices[k]);
								per_object_cbs[j].blend_flags[index0][index1] =
									static_cast<uint32_t>(prop_mesh.blend_flags[k]);
								per_object_cbs[j].texture_types[index0][index1] =
									static_cast<uint32_t>(model_texture_types[per_mesh_tex_ids[j][k]]) | (prop_mesh.texture_types[k] << 8);
							}
						}

						auto pixel_shader_type = PixelShaderType::OldModel;
						if (ffna_model_file_ptr->geometry_chunk.unknown_tex_stuff1.size() > 0)
						{
							pixel_shader_type = PixelShaderType::NewModel;
						}

						auto mesh_ids = map_renderer->AddProp(prop_meshes, per_object_cbs, i, pixel_shader_type);
						if (ffna_model_file_ptr->textures_parsed_correctly)
						{
							for (int l = 0; l < mesh_ids.size(); l++)
							{
								int mesh_id = mesh_ids[l];
								auto& mesh_texture_ids = per_mesh_tex_ids[l];

								map_renderer->GetMeshManager()->SetTexturesForMesh(
									mesh_id, map_renderer->GetTextureManager()->GetTextures(mesh_texture_ids),
									3);
							}
						}

						// Add prop index to map for later picking
						for (int l = 0; l < prop_meshes.size(); l++)
						{
							int object_id = per_object_cbs[l].object_id;
							int prop_index = i;

							object_id_to_prop_index.insert({ object_id, prop_index });
							object_id_to_submodel_index.emplace(object_id, l);
						}
					}
				}
			}
		}

		//for (int i = 0; i < selected_ffna_map_file.props_info_chunk.some_vertex_data.vertices.size(); i++) {
		//    const auto vertex = selected_ffna_map_file.props_info_chunk.some_vertex_data.vertices[i];
		//    map_renderer->AddBox(vertex.x, -vertex.z, vertex.y, 50);
		//}

		//for (int i = 0; i < selected_ffna_map_file.props_info_chunk.some_data1.array.size(); i++) {
		//    const auto vertex = selected_ffna_map_file.props_info_chunk.some_data1.array[i];
		//    map_renderer->AddBox(vertex.x, terrain->get_height_at(vertex.x, vertex.y) + 200, vertex.y, 50);
		//}

		//for (int i = 0; i < selected_ffna_map_file.chunk5.some_array.size(); i++) {
		//    auto color1 = CheckerboardTexture::GetColorForIndex(i + 21);
		//    auto color2 = CheckerboardTexture::GetColorForIndex(i + 21);

		//    const auto& vertices = selected_ffna_map_file.chunk5.some_array[i].vertices;
		//    if (vertices.size() < 3) {
		//        // Skip this iteration if there are fewer than 3 vertices.
		//        continue;
		//    }

		//    // Create the base Vertex3 using the first vertex and terrain height
		//    const auto& baseVertex2D = vertices[0];
		//    Vertex3 baseVertex3D = {
		//        baseVertex2D.x,
		//        terrain->get_height_at(baseVertex2D.x, baseVertex2D.y) + 20000,
		//        baseVertex2D.y
		//    };

		//    for (int j = 1; j < vertices.size() - 1; j++) {
		//        // Create Vertex3 for the two other vertices of the triangle
		//        const auto& vertex1_2D = vertices[j];
		//        Vertex3 vertex1_3D = {
		//            vertex1_2D.x,
		//            terrain->get_height_at(vertex1_2D.x, vertex1_2D.y) + 20000,
		//            vertex1_2D.y
		//        };

		//        const auto& vertex2_2D = vertices[j + 1];
		//        Vertex3 vertex2_3D = {
		//            vertex2_2D.x,
		//            terrain->get_height_at(vertex2_2D.x, vertex2_2D.y) + 20000,
		//            vertex2_2D.y
		//        };

		//        // Use AddTriangle3D to draw the triangle
		//        map_renderer->AddTriangle3D(baseVertex3D, vertex1_3D, vertex2_3D, 200, color1, color2);
		//    }
		//}


		//for (int i = 0; i < selected_ffna_map_file.chunk5.unknown2.size(); i += 2) {
		//    if (i + 1 < selected_ffna_map_file.chunk5.unknown2.size()) {
		//        const auto x = selected_ffna_map_file.chunk5.unknown2[i];
		//        const auto z = selected_ffna_map_file.chunk5.unknown2[i+1];
		//        map_renderer->AddBox(x, terrain->get_height_at(x, z) + 200, z, 300);
		//    }
		//}

		const auto& shore_filenames = selected_ffna_map_file.shore_filenames.array;
		std::vector<ID3D11ShaderResourceView*> shore_textures(shore_filenames.size(), nullptr);
		for (int i = 0; i < shore_filenames.size(); i++) {
			const auto& filename = shore_filenames[i].filename;
			auto decoded_filename = decode_filename(filename.id0, filename.id1);

			auto mft_entry_it = hash_index.find(decoded_filename);
			if (mft_entry_it != hash_index.end())
			{
				auto type = dat_manager->get_MFT()[mft_entry_it->second.at(0)].type;
				const DatTexture dat_texture = dat_manager->parse_ffna_texture_file(mft_entry_it->second.at(0));
				int texture_id = -1;
				if (dat_texture.width > 0 && dat_texture.height > 0) {

					auto HR = map_renderer->GetTextureManager()->CreateTextureFromRGBA(
						dat_texture.width, dat_texture.height, dat_texture.rgba_data.data(),
						&texture_id, decoded_filename);
					if (SUCCEEDED(HR) && texture_id >= 0) {
						shore_textures[i] = map_renderer->GetTextureManager()->GetTexture(texture_id);
					}
				}
			}
		}

		// Used as height for vertices out in the water.
		float water_vertex_height = 3;

		float u_factor = 0.00228122458f;

		std::vector<Mesh> shore_meshes;
		std::vector<PerObjectCB> shore_per_object_cbs;
		for (int i = 0; i < selected_ffna_map_file.shore_chunk.subchunks.size(); i++) {
			auto& sub_chunk_i = selected_ffna_map_file.shore_chunk.subchunks[i];

			// Used for the vertice closed to shore.
			float shore_height = -sub_chunk_i.shore_land_vertex_height; // negate due to gw using a different coordinate system than GWMB.
			water_vertex_height = shore_height;

			auto& vertices = sub_chunk_i.vertices;

			// Case for exactly 2 vertices
			if (vertices.size() == 2) {
				//XMFLOAT2 point1 = XMFLOAT2(vertices[0].x, vertices[0].y);
				//XMFLOAT2 point2 = XMFLOAT2(vertices[1].x, vertices[1].y);

				//generate_shore_mesh(point1, point2, terrain.get(), shore_meshes, shore_height);
			}
			// Case for more than 2 vertices
			else if (vertices.size() > 2) {
				std::vector<GWVertex> shore_vertices;
				std::vector<uint32_t> shore_indices;


				// Handle the first segment separately
				GWVertex offset_vertex_0 = get_shore_vertex_for_2_points(vertices[0], vertices[1], water_vertex_height);
				offset_vertex_0.tex_coord0 = { 0, 1 };

				// Add the first vertex and offset vertex
				shore_vertices.push_back(GWVertex({ vertices[0].x, shore_height, vertices[0].y }, { 0, 1, 0 }, { 0, 0 }));
				shore_vertices.push_back(offset_vertex_0);

				float shore_len = LengthXMFLOAT2({ vertices[0].x - vertices[1].x, vertices[0].y - vertices[1].y });

				// Add first triangle
				shore_indices.insert(shore_indices.end(), { 1,0,2 });

				// Handle intermediate vertices
				for (uint32_t j = 1; j < vertices.size() - 1; j++) {
					const auto& prev_vertex = vertices[j - 1];
					const auto& vertex = vertices[j];
					const auto& next_vertex = vertices[j + 1];

					// Compute the u texture coordinate. The v coordinate is always 0 for the shore vertex points and 1 for the offset points.
					float u = u_factor * shore_len;
					shore_len += LengthXMFLOAT2({ vertex.x - next_vertex.x, vertex.y - next_vertex.y });

					// Compute offset vertex (the one out in the water)
					GWVertex offset_vertex_j = get_shore_vertex_for_3_points(prev_vertex, vertex, next_vertex, water_vertex_height);
					offset_vertex_j.tex_coord0 = { u, 1 };

					// Add the current shore vertex.
					shore_vertices.push_back(GWVertex({ vertex.x, shore_height, vertex.y }, { 0, 1, 0 }, { u, 0 }));

					// And the offset one out in the water.
					shore_vertices.push_back(offset_vertex_j);

					uint32_t vertex_index = shore_vertices.size() - 2;

					// Add indices for 2 triangles
					shore_indices.insert(shore_indices.end(), { vertex_index - 1, vertex_index, vertex_index + 1, vertex_index + 1, vertex_index, vertex_index + 2 });
				}

				float u = u_factor * shore_len;

				// And compute the last shore offset vertex out in the water.
				GWVertex offset_vertex_last = get_shore_vertex_for_2_points(vertices.back(), vertices[vertices.size() - 2], water_vertex_height);
				offset_vertex_last.tex_coord0 = { u, 1 };

				// Add last shore vertex and offset vertex
				shore_vertices.push_back(GWVertex({ vertices.back().x, shore_height, vertices.back().y }, { 0, 1, 0 }, { u, 0 }));
				shore_vertices.push_back(offset_vertex_last);

				// Add last triangle
				uint32_t last_vertex_index = shore_vertices.size() - 1;
				shore_indices.insert(shore_indices.end(), { last_vertex_index - 2, last_vertex_index - 1, last_vertex_index });

				Mesh shore_mesh;
				shore_mesh.vertices = shore_vertices;
				shore_mesh.indices = shore_indices;
				shore_mesh.num_textures = 0;
				shore_mesh.should_cull = false;
				shore_mesh.blend_state = BlendState::AlphaBlend;
				shore_meshes.push_back(shore_mesh);

				PerObjectCB shore_per_object_data;
				for (int j = 0; j < shore_textures.size(); j++) {
					shore_per_object_data.texture_indices[j / 4][j % 4] = 0;
					shore_per_object_data.texture_types[j / 4][j % 4] = shore_textures[j] == nullptr ? 0xFF : 0;
					if (shore_textures[j] != nullptr) {
						shore_mesh.num_textures++;
					}
				}
				shore_per_object_data.shore_max_alpha = sub_chunk_i.max_alpha;
				shore_per_object_data.shore_wave_speed = sub_chunk_i.wave_speed;
				shore_per_object_data.num_uv_texture_pairs = shore_textures.size();

				shore_per_object_cbs.push_back(shore_per_object_data);
			}
		}

		map_renderer->SetShore(shore_meshes, shore_textures, shore_per_object_cbs, PixelShaderType::Shore);

		// Create pathfinding meshes from trapezoids
		if (selected_ffna_map_file.pathfinding_chunk.valid && selected_ffna_map_file.pathfinding_chunk.all_trapezoids.size() > 0) {
			std::vector<Mesh> pathfinding_meshes;
			std::vector<uint32_t> plane_sizes;
			const float pathfinding_height_offset = 50.0f;  // Height above terrain

			// Build plane sizes from planes data
			for (const auto& plane : selected_ffna_map_file.pathfinding_chunk.planes) {
				plane_sizes.push_back(plane.traps_count);
			}

			for (size_t i = 0; i < selected_ffna_map_file.pathfinding_chunk.all_trapezoids.size(); i++) {
				const auto& trap = selected_ffna_map_file.pathfinding_chunk.all_trapezoids[i];

				// Create quad mesh from trapezoid coordinates
				// trap.yt = top Y, trap.yb = bottom Y (world Z)
				// trap.xtl/xtr = top X coords, trap.xbl/xbr = bottom X coords (world X)
				float height_tl = terrain->get_height_at(trap.xtl, trap.yt) + pathfinding_height_offset;
				float height_tr = terrain->get_height_at(trap.xtr, trap.yt) + pathfinding_height_offset;
				float height_bl = terrain->get_height_at(trap.xbl, trap.yb) + pathfinding_height_offset;
				float height_br = terrain->get_height_at(trap.xbr, trap.yb) + pathfinding_height_offset;

				XMFLOAT3 pos_tl(trap.xtl, height_tl, trap.yt);
				XMFLOAT3 pos_tr(trap.xtr, height_tr, trap.yt);
				XMFLOAT3 pos_bl(trap.xbl, height_bl, trap.yb);
				XMFLOAT3 pos_br(trap.xbr, height_br, trap.yb);

				// Calculate normal (facing up)
				XMFLOAT3 normal(0.0f, 1.0f, 0.0f);

				// Texture coordinates
				XMFLOAT2 tex00(0.0f, 0.0f);
				XMFLOAT2 tex10(1.0f, 0.0f);
				XMFLOAT2 tex01(0.0f, 1.0f);
				XMFLOAT2 tex11(1.0f, 1.0f);

				Mesh mesh;
				mesh.vertices.push_back(GWVertex(pos_tl, normal, tex00));
				mesh.vertices.push_back(GWVertex(pos_tr, normal, tex10));
				mesh.vertices.push_back(GWVertex(pos_br, normal, tex11));
				mesh.vertices.push_back(GWVertex(pos_bl, normal, tex01));
				mesh.indices = { 0, 1, 2, 0, 2, 3 };
				mesh.blend_flags = { 8 };  // Alpha blend

				pathfinding_meshes.push_back(mesh);
			}

			map_renderer->SetPathfinding(pathfinding_meshes, plane_sizes, PixelShaderType::OldModel);
		}

		//for (int i = 0; i < selected_ffna_map_file.big_chunk.vertices0.size(); i++) {
		//    auto color1 = CheckerboardTexture::GetColorForIndex(3);
		//    auto color2 = CheckerboardTexture::GetColorForIndex(3);
		//
		//    const auto& vertex = selected_ffna_map_file.big_chunk.vertices0[i];
		//    map_renderer->AddBox(vertex.x, terrain->get_height_at(vertex.x, vertex.y) + 55000, vertex.y, 400, color1, color2);
		//}

		//for (int i = 0; i < selected_ffna_map_file.big_chunk.vertices1.size(); i++) {
		//    auto color1 = CheckerboardTexture::GetColorForIndex(2);
		//    auto color2 = CheckerboardTexture::GetColorForIndex(2);

		//    const auto& vertex = selected_ffna_map_file.big_chunk.vertices1[i];
		//    map_renderer->AddBox(vertex.x, terrain->get_height_at(vertex.x, vertex.y) + 45000, vertex.y, 800, color1, color2);
		//}

		//for (int i = 0; i < selected_ffna_map_file.big_chunk.vertices2.size(); i++) {
		//    auto color1 = CheckerboardTexture::GetColorForIndex(0);
		//    auto color2 = CheckerboardTexture::GetColorForIndex(0);

		//    const auto& vertex = selected_ffna_map_file.big_chunk.vertices2[i];
		//    map_renderer->AddBox(vertex.x, terrain->get_height_at(vertex.x, vertex.y) + 35000, vertex.y, 100, color1, color2);
		//}

		//for (int i = 0; i < selected_ffna_map_file.big_chunk.bcs0.size(); i++) {
		//    const auto* vertices = selected_ffna_map_file.big_chunk.bcs0[i].vertices;
		//    if (vertices) {
		//        auto color = CheckerboardTexture::GetColorForIndex(i);

		//        const auto v0 = vertices[0]; // y values
		//        const auto v1 = vertices[1]; // x values 1
		//        const auto v2 = vertices[2]; // x values 2

		//        Vertex3 TL{ v1.x, terrain->get_height_at(v1.x, v0.x) + 25000, v0.x };
		//        Vertex3 TR{ v1.y, terrain->get_height_at(v1.y, v0.x) + 25000, v0.x };

		//        Vertex3 BL{ v2.x, terrain->get_height_at(v2.x, v0.y) + 25000, v0.y };
		//        Vertex3 BR{ v2.y, terrain->get_height_at(v2.y, v0.y) + 25000, v0.y };

		//        map_renderer->AddTrapezoid3D(TL, TR, BL, BR, 100, color, color);

		//        //color = CheckerboardTexture::GetColorForIndex(0);
		//        //map_renderer->AddBox(v1.x, terrain->get_height_at(v1.x, v0.x) + 4000, v0.x, 200, color, color);
		//        //map_renderer->AddBox(v1.y, terrain->get_height_at(v1.y, v0.x) + 4000, v0.x, 200, color, color);

		//        //map_renderer->AddBox(v2.x, terrain->get_height_at(v2.x, v0.y) + 4000, v0.y, 200, color, color);
		//        //map_renderer->AddBox(v2.y, terrain->get_height_at(v2.y, v0.y) + 4000, v0.y, 200, color, color);
		//    }
		//}
	}

	break;
	default:
		break;
	}

	return success;
}

std::string truncate_text_with_ellipsis(const std::string& text, float maxWidth);
int custom_stoi(const std::string& input);
std::string to_lower(const std::string& input);

void draw_data_browser(DATManager* dat_manager, MapRenderer* map_renderer, const bool dat_manager_changed, const std::unordered_set<uint32_t>& dat_compare_filter_result, const bool dat_compare_filter_result_changed,
	std::vector<std::vector<std::string>>& csv_data, bool custom_file_info_changed)
{
	static std::vector<DatBrowserItem> items;
	static std::vector<DatBrowserItem> filtered_items;

	static std::unordered_map<int, std::vector<int>> id_index;
	static std::unordered_map<int, std::vector<int>> hash_index;
	static std::unordered_map<int, std::vector<int>> file_id_0_index;
	static std::unordered_map<int, std::vector<int>> file_id_1_index;
	static std::unordered_map<FileType, std::vector<int>> type_index;

	static std::unordered_map<int, std::vector<int>> map_id_index;
	static std::unordered_map<std::string, std::vector<int>> name_index;
	static std::unordered_map<bool, std::vector<int>> pvp_index;
	static std::unordered_map<int, std::vector<int>> murmurhash3_index;
	static std::unordered_map<uint32_t, std::vector<int>> chunk_id_index;
	static std::set<uint32_t> all_unique_chunk_ids;
	static std::set<uint32_t> selected_chunk_ids;
	static std::unordered_map<uint32_t, int> chunk_id_counts;

	static std::unordered_map<int, CustomFileInfoEntry> custom_file_info_map;

	if (custom_file_info_changed) {
		for (int i = 1; i < csv_data.size(); i++) {
			const auto& row = csv_data[i];
			CustomFileInfoEntry new_entry;
			if (row[0][0] == '0' && std::tolower(row[0][1]) == 'x') {
				new_entry.hash = std::stoul(row[0], 0, 16);
			}
			else {
				new_entry.hash = std::stoul(row[0]);
			}

			for (auto name_token : std::views::split(row[1], '|')) {
				if (!name_token.empty()) {
					std::string name(&*name_token.begin(), std::ranges::distance(name_token));
					new_entry.names.push_back(name);
				}
			}

			for (auto map_id_token : std::views::split(row[3], '|')) {
				if (!map_id_token.empty()) {
					std::string map_id(&*map_id_token.begin(), std::ranges::distance(map_id_token));
					new_entry.map_ids.push_back(std::stoi(map_id));
				}
			}

			new_entry.is_pvp = row[6] == "yes";

			custom_file_info_map.insert_or_assign(new_entry.hash, new_entry);
		}
	}

	if (dat_manager_changed || custom_file_info_changed) {
		items.clear();
		filtered_items.clear();
		id_index.clear();
		hash_index.clear();
		file_id_0_index.clear();
		file_id_1_index.clear();
		type_index.clear();
		map_id_index.clear();
		name_index.clear();
		pvp_index.clear();
		chunk_id_index.clear();
		all_unique_chunk_ids.clear();
		selected_chunk_ids.clear();
		chunk_id_counts.clear();
	}

	if (!GuiGlobalConstants::is_dat_browser_resizeable)
	{
		auto dat_browser_window_size =
			ImVec2(ImGui::GetIO().DisplaySize.x -
				(GuiGlobalConstants::left_panel_width + GuiGlobalConstants::panel_padding * 2) -
				(GuiGlobalConstants::right_panel_width + GuiGlobalConstants::panel_padding * 2),
				300);
		ImGui::SetNextWindowSize(dat_browser_window_size);
	}

	if (!GuiGlobalConstants::is_dat_browser_movable) {
		auto dat_browser_window_pos =
			ImVec2(GuiGlobalConstants::left_panel_width + GuiGlobalConstants::panel_padding * 2,
				GuiGlobalConstants::menu_bar_height + GuiGlobalConstants::panel_padding);
		ImGui::SetNextWindowPos(dat_browser_window_pos);
	}


	if (GuiGlobalConstants::is_dat_browser_open) {
		if (ImGui::Begin("Browse .dat file contents", &GuiGlobalConstants::is_dat_browser_open, ImGuiWindowFlags_NoFocusOnAppearing)) {
			// Create item list
			if (items.size() == 0)
			{
				const auto& entries = dat_manager->get_MFT();
				for (int i = 0; i < entries.size(); i++)
				{
					const auto& entry = entries[i];
					int filename_id_0 = 0;
					int filename_id_1 = 0;

					// Update filename_id_0 and filename_id_1 with the proper values.
					encode_filehash(entry.Hash, filename_id_0, filename_id_1);

					DatBrowserItem new_item{
						i, entry.Hash, static_cast<FileType>(entry.type), entry.Size, entry.uncompressedSize, filename_id_0, filename_id_1, {}, {}, {}, entry.murmurhash3, entry.chunk_ids
					};
					auto custom_file_info_it = custom_file_info_map.find(entry.Hash);
					if (custom_file_info_it == custom_file_info_map.end()) {
						// Files with file_id == 0 uses murmurhash3 instead when saved to custom file
						custom_file_info_it = custom_file_info_map.find(entry.murmurhash3);
					}

					if (entry.type == FFNA_Type3)
					{
						if (custom_file_info_it != custom_file_info_map.end()) {
							new_item.names = custom_file_info_it->second.names;
							new_item.map_ids = custom_file_info_it->second.map_ids;
							new_item.is_pvp = { custom_file_info_it->second.is_pvp };
						}
						else {
							auto it = constant_maps_info.find(entry.Hash);
							if (it != constant_maps_info.end())
							{
								for (const auto& map : it->second)
								{
									new_item.map_ids.push_back(map.map_id);
									new_item.names.push_back(map.map_name);
									new_item.is_pvp.push_back(map.is_pvp);
								}
							}
						}
					}
					else {
						if (custom_file_info_it != custom_file_info_map.end()) {
							new_item.names = custom_file_info_it->second.names;
						}
					}

					items.push_back(new_item);
				}
				filtered_items = items;
			}

			if (items.size() != 0 && id_index.empty())
			{
				for (int i = 0; i < items.size(); i++)
				{
					const auto& item = items[i];
					id_index[item.id].push_back(i);
					hash_index[item.hash].push_back(i);
					type_index[item.type].push_back(i);
					file_id_0_index[item.file_id_0].push_back(i);
					file_id_1_index[item.file_id_1].push_back(i);
					murmurhash3_index[item.murmurhash3].push_back(i);

					for (const auto map_id : item.map_ids) { map_id_index[map_id].push_back(i); }
					for (const auto& name : item.names)
					{
						if (name != "" && name != "-")
							name_index[name].push_back(i);
					}
					for (const auto is_pvp : item.is_pvp) { pvp_index[is_pvp].push_back(i); }
					for (const auto chunk_id : item.chunk_ids)
					{
						chunk_id_index[chunk_id].push_back(i);
						all_unique_chunk_ids.insert(chunk_id);
						chunk_id_counts[chunk_id]++;
					}
				}
			}

			// Set after filtering is complete.
			static std::string curr_id_filter = "";
			static std::string curr_hash_filter = "";
			static FileType curr_type_filter = NONE;
			static std::string curr_map_id_filter = "";
			static std::string curr_name_filter = "";
			static int curr_pvp_filter = -1;
			static std::string curr_filename_filter = "";
			static std::string curr_murmurhash3_filter = "";
			static std::set<uint32_t> curr_chunk_filter;

			// The values set by the user in the GUI
			static std::string id_filter_text;
			static std::string hash_filter_text;
			static FileType type_filter_value = NONE;
			static std::string map_id_filter_text;
			static std::string name_filter_text;
			static int pvp_filter_value = -1; // -1 means no filter, 0 means false, 1 means true
			static std::string filename_filter_text;
			static std::string murmurhash3_filter_text;

			static bool filter_update_required = true;

			if (dat_manager_changed || custom_file_info_changed) {
				filter_update_required = true;
			}

			if (curr_id_filter != id_filter_text)
			{
				curr_id_filter = id_filter_text;
				filter_update_required = true;
			}

			if (curr_hash_filter != hash_filter_text)
			{
				curr_hash_filter = hash_filter_text;
				filter_update_required = true;
			}

			if (curr_type_filter != type_filter_value)
			{
				curr_type_filter = type_filter_value;
				filter_update_required = true;
			}

			if (curr_map_id_filter != map_id_filter_text)
			{
				curr_map_id_filter = map_id_filter_text;
				filter_update_required = true;
			}

			if (curr_name_filter != name_filter_text)
			{
				curr_name_filter = name_filter_text;
				filter_update_required = true;
			}

			if (curr_pvp_filter != pvp_filter_value)
			{
				curr_pvp_filter = pvp_filter_value;
				filter_update_required = true;
			}

			if (curr_filename_filter != filename_filter_text)
			{
				curr_filename_filter = filename_filter_text;
				filter_update_required = true;
			}

			if (curr_murmurhash3_filter != murmurhash3_filter_text)
			{
				curr_murmurhash3_filter = murmurhash3_filter_text;
				filter_update_required = true;
			}

			if (curr_chunk_filter != selected_chunk_ids)
			{
				curr_chunk_filter = selected_chunk_ids;
				filter_update_required = true;
			}

			if (dat_compare_filter_result_changed) {
				filter_update_required = true;
			}

			// Only re-run the filter when the user changed filter params in the GUI.
			bool filter_updated = filter_update_required;
			if (filter_update_required)
			{

				filter_update_required = false;

				filtered_items.clear();

				std::unordered_set<int> intersection;

				if (!id_filter_text.empty())
				{
					int id_filter_value = custom_stoi(id_filter_text);
					if (id_index.contains(id_filter_value))
					{
						intersection.insert(id_index[id_filter_value].begin(), id_index[id_filter_value].end());
					}
				}

				if (!hash_filter_text.empty())
				{
					int hash_filter_value = custom_stoi(hash_filter_text);
					if (hash_index.contains(hash_filter_value))
					{
						if (id_filter_text.empty())
						{
							intersection.insert(hash_index[hash_filter_value].begin(),
								hash_index[hash_filter_value].end());
						}
						else
						{
							std::unordered_set<int> new_intersection;
							for (int id : hash_index[hash_filter_value])
							{
								if (intersection.contains(id)) { new_intersection.insert(id); }
							}
							intersection = std::move(new_intersection);
						}
					}
				}

				if (!filename_filter_text.empty())
				{
					int filename_filter_value = custom_stoi(filename_filter_text);
					uint16_t id0 = filename_filter_value & 0xFFFF;
					uint16_t id1 = (filename_filter_value >> 16) & 0xFFFF;

					bool is_full_filename_hash = id0 > 0xFF && id1 > 0xFF;

					if (!is_full_filename_hash) {
						if (file_id_0_index.contains(id0))
						{
							apply_filter(file_id_0_index[id0], intersection);
						}

						if (file_id_1_index.contains(id1))
						{
							apply_filter(file_id_1_index[id1], intersection);
						}
					}
					else {
						if (file_id_0_index.contains(id0) && file_id_1_index.contains(id1))
						{
							apply_filter(file_id_0_index[id0], intersection);
							apply_filter(file_id_1_index[id1], intersection);
						}
						else if (file_id_0_index.contains(id1) && file_id_1_index.contains(id0)) {
							apply_filter(file_id_0_index[id1], intersection);
							apply_filter(file_id_1_index[id0], intersection);
						}
					}
				}

				if (type_filter_value != NONE)
				{
					if (id_filter_text.empty() && hash_filter_text.empty())
					{
						intersection.insert(type_index[type_filter_value].begin(),
							type_index[type_filter_value].end());
					}
					else
					{
						std::unordered_set<int> new_intersection;
						for (int id : type_index[type_filter_value])
						{
							if (intersection.contains(id)) { new_intersection.insert(id); }
						}
						intersection = std::move(new_intersection);
					}
				}

				if (!map_id_filter_text.empty())
				{
					int map_id_filter_value = custom_stoi(map_id_filter_text);
					if (map_id_index.contains(map_id_filter_value))
					{
						apply_filter(map_id_index[map_id_filter_value], intersection);
					}
				}

				if (!name_filter_text.empty())
				{
					std::vector<int> matching_indices;
					std::string name_filter_text_lower = to_lower(name_filter_text);

					for (const auto& name_entry : name_index)
					{
						std::string name_entry_lower = to_lower(name_entry.first);
						if (name_entry_lower.find(name_filter_text_lower) != std::string::npos)
						{
							matching_indices.insert(matching_indices.end(), name_entry.second.begin(),
								name_entry.second.end());
						}
					}
					if (!matching_indices.empty()) { apply_filter(matching_indices, intersection); }
				}

				if (pvp_filter_value != -1)
				{
					bool pvp_filter_bool = pvp_filter_value == 1;
					apply_filter(pvp_index[pvp_filter_bool], intersection);
				}

				if (!murmurhash3_filter_text.empty())
				{
					int murmurhash3_filter_value = custom_stoi(murmurhash3_filter_text);
					if (murmurhash3_index.contains(murmurhash3_filter_value))
					{
						if (intersection.empty())
						{
							intersection.insert(murmurhash3_index[murmurhash3_filter_value].begin(),
								murmurhash3_index[murmurhash3_filter_value].end());
						}
						else
						{
							std::unordered_set<int> new_intersection;
							for (int id : murmurhash3_index[murmurhash3_filter_value])
							{
								if (intersection.contains(id)) { new_intersection.insert(id); }
							}
							intersection = std::move(new_intersection);
						}
					}
				}

				// Chunk ID filter (OR logic - match any selected chunk)
				if (!selected_chunk_ids.empty())
				{
					std::vector<int> matching_indices;
					for (uint32_t chunk_id : selected_chunk_ids)
					{
						if (chunk_id_index.contains(chunk_id))
						{
							for (int idx : chunk_id_index[chunk_id])
							{
								matching_indices.push_back(idx);
							}
						}
					}
					// Remove duplicates (file may have multiple selected chunks)
					std::sort(matching_indices.begin(), matching_indices.end());
					matching_indices.erase(std::unique(matching_indices.begin(), matching_indices.end()), matching_indices.end());
					apply_filter(matching_indices, intersection);
				}

				if (id_filter_text.empty() && hash_filter_text.empty() && type_filter_value == NONE &&
					map_id_filter_text.empty() && name_filter_text.empty() && pvp_filter_value == -1 && filename_filter_text.empty() &&
					murmurhash3_filter_text.empty() && selected_chunk_ids.empty()) {
					for (const auto& item : items) {
						if (dat_compare_filter_result.contains(item.murmurhash3) || dat_compare_filter_result.empty())
						{
							filtered_items.push_back(item);
						}
					}
				}

				else {
					for (const auto& id : intersection)
					{
						const auto& item = items[id];
						if (dat_compare_filter_result.contains(item.murmurhash3) || dat_compare_filter_result.empty()) {
							filtered_items.push_back(item);
						}
					}
				}

				// Set them equal so that the filter won't run again until the filter changes.
				curr_id_filter = id_filter_text;
				curr_hash_filter = hash_filter_text;
				curr_type_filter = type_filter_value;
				curr_map_id_filter = map_id_filter_text;
				curr_name_filter = name_filter_text;
				curr_pvp_filter = pvp_filter_value;
				curr_filename_filter = filename_filter_text;
				curr_murmurhash3_filter = murmurhash3_filter_text;
				curr_chunk_filter = selected_chunk_ids;
			}

			// Filter table
			// Render the filter inputs and the table
			ImGui::Columns(8);
			ImGui::Text("Id:");
			ImGui::SameLine();
			ImGui::InputText("##IdFilter", &id_filter_text);
			ImGui::NextColumn();

			ImGui::Text("File ID:");
			ImGui::SameLine();
			ImGui::InputText("##HashFilter", &hash_filter_text);
			ImGui::NextColumn();

			ImGui::Text("Filename");
			ImGui::SameLine();
			ImGui::InputText("##FilenameFilter", &filename_filter_text);
			ImGui::NextColumn();

			ImGui::Text("Name:");
			ImGui::SameLine();
			ImGui::InputText("##NameFilter", &name_filter_text);
			ImGui::NextColumn();

			ImGui::Text("Map ID:");
			ImGui::SameLine();
			ImGui::InputText("##MapID", &map_id_filter_text);
			ImGui::NextColumn();

			ImGui::Text("Type:");
			ImGui::SameLine();
			ImGui::Combo("##EnumFilter", reinterpret_cast<int*>(&type_filter_value), type_strings, 25);
			ImGui::NextColumn();

			ImGui::Text("Murmur3:");
			ImGui::SameLine();
			ImGui::InputText("##Murmur3Filter", &murmurhash3_filter_text);
			ImGui::NextColumn();

			// Chunk filter - multi-select dropdown
			ImGui::Text("Chunks:");
			ImGui::SameLine();
			{
				// Build preview text
				std::string chunk_filter_preview;
				if (selected_chunk_ids.empty()) {
					chunk_filter_preview = "None";
				} else if (selected_chunk_ids.size() == 1) {
					chunk_filter_preview = std::format("0x{:X}", *selected_chunk_ids.begin());
				} else {
					chunk_filter_preview = std::format("{} selected", selected_chunk_ids.size());
				}

				if (ImGui::BeginCombo("##ChunkFilter", chunk_filter_preview.c_str()))
				{
					// Clear all button
					if (ImGui::Button("Clear All"))
					{
						selected_chunk_ids.clear();
					}
					ImGui::Separator();

					for (uint32_t chunk_id : all_unique_chunk_ids)
					{
						bool is_selected = selected_chunk_ids.contains(chunk_id);
						auto label = std::format("0x{:X} ({} files)", chunk_id, chunk_id_counts[chunk_id]);
						if (ImGui::Checkbox(label.c_str(), &is_selected))
						{
							if (is_selected) {
								selected_chunk_ids.insert(chunk_id);
							} else {
								selected_chunk_ids.erase(chunk_id);
							}
						}
					}
					ImGui::EndCombo();
				}
			}
			ImGui::Columns(1);

			ImGui::Separator();

			ImGui::Text("Filtered items: %d", filtered_items.size());
			ImGui::SameLine();
			ImGui::Text("Total items: %d", items.size());

			// Options
			static ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
				ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti |
				ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
				ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_ScrollY;

			if (ImGui::BeginTable("data browser", 11, flags))
			{
				// Declare columns
				// We use the "user_id" parameter of TableSetupColumn() to specify a user id that will be stored in the sort specifications.
				// This is so our sort function can identify a column given our own identifier. We could also identify them based on their index!
				// Demonstrate using a mixture of flags among available sort-related flags:
				// - ImGuiTableColumnFlags_DefaultSort
				// - ImGuiTableColumnFlags_NoSort / ImGuiTableColumnFlags_NoSortAscending / ImGuiTableColumnFlags_NoSortDescending
				// - ImGuiTableColumnFlags_PreferSortAscending / ImGuiTableColumnFlags_PreferSortDescending
				ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_DefaultSort, 0.0f, DatBrowserItemColumnID_id);
				ImGui::TableSetupColumn("File ID", 0, 0.0f, DatBrowserItemColumnID_hash);
				ImGui::TableSetupColumn("Filename", 0, 0.0f, DatBrowserItemColumnID_filename);
				ImGui::TableSetupColumn("Name", 0, 0.0f, DatBrowserItemColumnID_name);
				ImGui::TableSetupColumn("Type", 0, 0.0f, DatBrowserItemColumnID_type);
				ImGui::TableSetupColumn("Size", 0, 0.0f, DatBrowserItemColumnID_size);
				ImGui::TableSetupColumn("Decompressed size", 0, 0.0f, DatBrowserItemColumnID_decompressed_size);
				ImGui::TableSetupColumn("Map id", 0, 0.0f, DatBrowserItemColumnID_map_id);
				ImGui::TableSetupColumn("PvP", 0, 0.0f, DatBrowserItemColumnID_is_pvp);
				ImGui::TableSetupColumn("murmur3", 0, 0.0f, DatBrowserItemColumnID_murmurhash3);
				ImGui::TableSetupColumn("Chunks", 0, 0.0f, DatBrowserItemColumnID_chunk_ids);
				ImGui::TableSetupScrollFreeze(0, 1); // Make row always visible

				ImGui::TableHeadersRow();

				// Sort our data if sort specs have been changed!
				ImGuiTableSortSpecs* sorts_specs = ImGui::TableGetSortSpecs();
				if (sorts_specs)
					if (dat_manager_changed || custom_file_info_changed || filter_updated) {
						sorts_specs->SpecsDirty = true;
					}

				if (sorts_specs->SpecsDirty)
				{
					DatBrowserItem::s_current_sort_specs =
						sorts_specs; // Store in variable accessible by the sort function.
					if (filtered_items.size() > 1)
						qsort(&filtered_items[0], filtered_items.size(), sizeof(filtered_items[0]),
							DatBrowserItem::CompareWithSortSpecs);
					DatBrowserItem::s_current_sort_specs = nullptr;
					sorts_specs->SpecsDirty = false;
				}

				// Demonstrate using clipper for large vertical lists
				ImGuiListClipper clipper;
				clipper.Begin(filtered_items.size());

				static int selected_item_id = -1;
				ImGuiSelectableFlags selectable_flags =
					ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;

				while (clipper.Step())
					for (int row_n = clipper.DisplayStart; row_n < clipper.DisplayEnd; row_n++)
					{
						DatBrowserItem& item = filtered_items[row_n];

						const bool item_is_selected = selected_item_id == item.id;

						auto label = std::format("{}", item.id);

						// Display a data item
						ImGui::PushID(item.id);
						ImGui::TableNextRow();
						ImGui::TableNextColumn();

						// Check if this is the row to highlight
						if (item.hash > 0 && item.hash == selected_item_hash) {
							// Set the background color for this row
							ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImGuiCol_HeaderHovered));
						}

						if (ImGui::Selectable(label.c_str(), item_is_selected, selectable_flags))
						{
							if (ImGui::GetIO().KeyCtrl) {}
							else
							{
								parse_file(dat_manager, item.id, map_renderer, hash_index);
								selected_item_id = item.id;
								selected_item_hash = item.hash;
								selected_item_murmurhash3 = item.murmurhash3;
							}
						}
						// If the item is focused (highlighted by navigation), select it immediately
						if (ImGui::IsItemFocused() && selected_item_id != item.id) {
							if (selected_item_id != item.id) {
								parse_file(dat_manager, item.id, map_renderer, hash_index);
								selected_item_id = item.id;
								selected_item_hash = item.hash;
								selected_item_murmurhash3 = item.murmurhash3;
							}

							// Check the direction of focus movement and adjust the scroll position
							if (last_focused_item_index != -1) {
								float row_height = ImGui::GetTextLineHeightWithSpacing();

								if (row_n < last_focused_item_index) {
									// Focus moved up, scroll up
									ImGui::SetScrollY(ImGui::GetScrollY() - row_height);
								}
								else if (row_n > last_focused_item_index) {
									// Focus moved down, scroll down
									ImGui::SetScrollY(ImGui::GetScrollY() + row_height);
								}
							}

							last_focused_item_index = row_n; // Update the last focused item index
						}

						if (dat_manager_changed || custom_file_info_changed) {
							// Find the index of the item with item.hash == selected_item_hash or item.murmurhash3 == selected_item_hash
							int item_index = -1;
							for (int i = 0; i < filtered_items.size(); ++i) {
								if (filtered_items[i].hash == selected_item_hash) {
									item_index = i;
									break;
								}
							}

							if (item_index == -1) {
								for (int i = 0; i < filtered_items.size(); ++i) {
									if (filtered_items[i].murmurhash3 == selected_item_hash) { // note multiple files can share the same murmurhash3
										item_index = i;
										break;
									}
								}
							}

							// If the item was found
							if (item_index != -1) {
								// Calculate the height of a single row (this is an approximation)
								float row_height = ImGui::GetTextLineHeightWithSpacing();

								// Calculate the number of visible rows in the table
								float visible_rows = ImGui::GetWindowHeight() / row_height;

								// Calculate the position to scroll to
								// We want the selected row to be at the top of the table, so we scroll to its position minus half the visible rows
								float scroll_pos = (item_index - visible_rows / 2) * row_height;

								// Clamp the scroll position to the valid range
								scroll_pos = std::max(0.0f, std::min(scroll_pos, ImGui::GetScrollMaxY()));

								// Scroll to the item
								ImGui::SetScrollY(scroll_pos);
							}
						}

						//Add context menu on right clicking item in table
						if (ImGui::BeginPopupContextItem("ItemContextMenu"))
						{
							if (ImGui::MenuItem("Save decompressed data to file"))
							{
								std::wstring savePath = OpenFileDialog(std::format(L"0x{:X}", item.hash), L"gwraw");
								if (!savePath.empty()) { dat_manager->save_raw_decompressed_data_to_file(item.id, savePath); }
							}

							if (item.type == SOUND || item.type == AMP)
							{
								if (ImGui::MenuItem("Save to mp3"))
								{
									std::wstring savePath = OpenFileDialog(std::format(L"0x{:X}", item.hash), L"mp3");
									if (!savePath.empty()) { dat_manager->save_raw_decompressed_data_to_file(item.id, savePath); }
								}
							}
							else if (item.type == FFNA_Type2)
							{
								if (ImGui::MenuItem("Export model as JSON"))
								{
									std::wstring saveDir = OpenDirectoryDialog();
									if (!saveDir.empty())
									{
										// Use std::format to create the filename
										std::wstring filename = std::format(L"model_0x{:X}_gwmb.json", item.hash);


										// Export the model to the chosen path
										model_exporter::export_model(saveDir, filename, item.id, dat_manager, hash_index, map_renderer->GetTextureManager());
									}
								}
								if (ImGui::MenuItem("Export Mesh"))
								{
									std::wstring savePath =
										OpenFileDialog(std::format(L"model_mesh_0x{:X}", item.hash), L"obj");
									if (!savePath.empty())
									{
										parse_file(dat_manager, item.id, map_renderer, hash_index);
										const auto obj_file_str = write_obj_str(prop_meshes);

										std::ofstream outFile(savePath);
										if (outFile.is_open())
										{
											outFile << obj_file_str;
											outFile.close();
										}
										else
										{
											// Error handling
										}
									}
								}

								if (ImGui::MenuItem("Export Submeshes Individually"))
								{
									std::wstring saveDir = OpenDirectoryDialog();
									if (!saveDir.empty())
									{
										parse_file(dat_manager, item.id, map_renderer, hash_index);
										for (size_t prop_mesh_index = 0; prop_mesh_index < prop_meshes.size();
											++prop_mesh_index)
										{
											const auto& prop_mesh = prop_meshes[prop_mesh_index];
											const auto obj_file_str = write_obj_str(&prop_mesh);

											// Generate unique file name
											std::wstring filename = std::format(L"model_mesh_0x{:X}_{}.obj", item.hash, prop_mesh_index);

											// Append the filename to the saveDir
											std::wstring savePath = saveDir + L"\\" + filename;

											std::ofstream outFile(savePath);
											if (outFile.is_open())
											{
												outFile << obj_file_str;
												outFile.close();
											}
											else
											{
												// Error handling
											}
										}
									}
								}

								if (ImGui::MenuItem("Export model textures (.png)"))
								{
									std::wstring saveDir = OpenDirectoryDialog();
									if (!saveDir.empty())
									{
										parse_file(dat_manager, item.id, map_renderer, hash_index);

										for (int tex_index = 0; tex_index < selected_ffna_model_file.texture_filenames_chunk.
											texture_filenames.
											size(); tex_index++)
										{
											const auto& texture_filename = selected_ffna_model_file.texture_filenames_chunk.
												texture_filenames[tex_index];

											auto decoded_filename = decode_filename(texture_filename.id0, texture_filename.id1);
											int texture_id = map_renderer->GetTextureManager()->
												GetTextureIdByHash(decoded_filename);

											std::wstring filename = std::format(L"model_0x{:X}_tex_index{}_texture_0x{:X}.png",
												item.hash, tex_index, decoded_filename);

											// Append the filename to the saveDir
											std::wstring savePath = saveDir + L"\\" + filename;

											ID3D11ShaderResourceView* texture =
												map_renderer->GetTextureManager()->GetTexture(texture_id);
											if (SaveTextureToPng(texture, savePath, map_renderer->GetTextureManager()))
											{
												// Success
											}
											else
											{
												// Error handling }
											}
										}
									}
								}
								if (ImGui::MenuItem("Export model textures (.dds) BC1"))
								{
									const auto compression_format = CompressionFormat::BC1;
									ExportDDS(dat_manager, item.id, item.hash, map_renderer, hash_index, compression_format);
								}
								if (ImGui::MenuItem("Export model textures (.dds) BC3"))
								{
									const auto compression_format = CompressionFormat::BC3;
									ExportDDS(dat_manager, item.id, item.hash, map_renderer, hash_index, compression_format);
								}

								if (ImGui::MenuItem("Export model textures (.dds) BC5"))
								{
									const auto compression_format = CompressionFormat::BC5;
									ExportDDS(dat_manager, item.id, item.hash, map_renderer, hash_index, compression_format);
								}

								if (ImGui::MenuItem("Export model textures (.dds) no compression"))
								{
									const auto compression_format = CompressionFormat::None;
									ExportDDS(dat_manager, item.id, item.hash, map_renderer, hash_index, compression_format);
								}
							}
							else if (item.type == FFNA_Type3)
							{
								if (ImGui::MenuItem("Export full map"))
								{
									std::wstring savePath = OpenDirectoryDialog();
									if (!savePath.empty())
									{
										// Create a new directory name using the item's hash
										std::wstring newDirName = L"gwmb_map_" + std::to_wstring(item.hash);

										// Append the new directory name to the existing path
										std::filesystem::path newDirPath = std::filesystem::path(savePath) / newDirName;

										// Check if the directory exists and if not, create it
										if (!exists(newDirPath))
										{
											create_directory(newDirPath);
										}

										map_exporter::export_map(newDirPath, item.hash, item.id, dat_manager, hash_index, map_renderer->GetTextureManager());
									}
								}
								else if (ImGui::MenuItem("Export Terrain Mesh as .obj"))
								{
									std::wstring savePath =
										OpenFileDialog(std::format(L"height_map_0x{:X}", item.hash), L"obj");
									if (!savePath.empty())
									{
										parse_file(dat_manager, item.id, map_renderer, hash_index);
										const auto& terrain_mesh = terrain.get()->get_mesh();
										const auto obj_file_str = write_obj_str(terrain_mesh);

										std::string savePathStr(savePath.begin(), savePath.end());

										std::ofstream outFile(savePathStr);
										if (outFile.is_open())
										{
											outFile << obj_file_str;
											outFile.close();
										}
										else
										{
											// Error handling
										}
									}
								}
								else if (ImGui::MenuItem("Export heightmap as .tiff"))
								{
									std::wstring savePath = OpenFileDialog(std::format(L"terrain_height_map_0x{:X}", item.hash),
										L"tiff");
									if (!savePath.empty())
									{
										parse_file(dat_manager, item.id, map_renderer, hash_index);
										const auto& terrain_mesh = terrain.get()->get_heightmap_grid();
										// Assuming the accessor function is available.

										// Convert the savePath to a string
										std::string save_path_str(savePath.begin(), savePath.end());

										// Write the BMP file
										if (!write_heightmap_tiff(terrain_mesh, save_path_str.c_str()))
										{
											// Error handling
										}
									}
								}
								else if (ImGui::MenuItem("Export terrain texture indices as .tiff"))
								{
									std::wstring savePath = OpenFileDialog(std::format(L"terrain_tex_indices_0x{:X}", item.hash),
										L"tiff");
									if (!savePath.empty())
									{
										parse_file(dat_manager, item.id, map_renderer, hash_index);
										const auto& terrain_texture_indices = terrain.get()->get_texture_index_grid();
										// Assuming the accessor function is available.

										// Convert the savePath to a string
										std::string save_path_str(savePath.begin(), savePath.end());

										// Write the BMP file
										if (!write_terrain_ints_tiff(terrain_texture_indices, save_path_str.c_str()))
										{
											// Error handling
										}
									}
								}
								else if (ImGui::MenuItem("Export terrain shadow map as .tiff"))
								{
									std::wstring savePath = OpenFileDialog(std::format(L"terrain_shadow_map_0x{:X}", item.hash),
										L"tiff");
									if (!savePath.empty())
									{
										parse_file(dat_manager, item.id, map_renderer, hash_index);
										const auto& terrain_unknown = terrain.get()->get_terrain_shadow_map_grid();
										// Assuming the accessor function is available.

										// Convert the savePath to a string
										std::string save_path_str(savePath.begin(), savePath.end());

										// Write the BMP file
										if (!write_terrain_ints_tiff(terrain_unknown, save_path_str.c_str()))
										{
											// Error handling
										}
									}
								}
							}
							else if (item.type == DDS) {
								if (ImGui::MenuItem("Export texture as DDS")) {
									parse_file(dat_manager, item.id, map_renderer, hash_index);
									std::wstring savePath = OpenFileDialog(std::format(L"terrain_tex_indices_0x{:X}", item.hash),
										L"dds");
									if (!savePath.empty())
									{
										dat_manager->save_raw_decompressed_data_to_file(item.id, savePath);
									}
								}
								else if (ImGui::MenuItem("Export texture as png")) {
									parse_file(dat_manager, item.id, map_renderer, hash_index);
									std::wstring savePath = OpenFileDialog(std::format(L"terrain_tex_indices_0x{:X}", item.hash),
										L"png");
									if (!savePath.empty())
									{
										int texture_id = map_renderer->GetTextureManager()->GetTextureIdByHash(item.hash);

										std::wstring filename = std::format(L"texture_0x{:X}.png", item.hash);

										ID3D11ShaderResourceView* texture =
											map_renderer->GetTextureManager()->GetTexture(texture_id);
										if (SaveTextureToPng(texture, savePath, map_renderer->GetTextureManager()))
										{
											// Success
										}
										else
										{
											// Error handling }
										}
									}
								}
							}
							else if (item.type >= ATEXDXT1 && item.type <= ATTXDXTL &&
								item.type != ATEXDXTA && item.type != ATTXDXTA)
							{
								if (ImGui::MenuItem("Export texture as DDS (BC1)")) {
									const auto compression_format = CompressionFormat::BC1;
									ExportDDS2(dat_manager, item, map_renderer, hash_index, compression_format);
								}
								else if (ImGui::MenuItem("Export texture as DDS (BC3)")) {
									const auto compression_format = CompressionFormat::BC3;
									ExportDDS2(dat_manager, item, map_renderer, hash_index, compression_format);
								}
								else if (ImGui::MenuItem("Export texture as DDS (BC5)")) {
									const auto compression_format = CompressionFormat::BC5;
									ExportDDS2(dat_manager, item, map_renderer, hash_index, compression_format);
								}
								else if (ImGui::MenuItem("Export texture as DDS (No compression)")) {
									const auto compression_format = CompressionFormat::None;
									ExportDDS2(dat_manager, item, map_renderer, hash_index, compression_format);
								}
								else if (ImGui::MenuItem("Export texture as png")) {

									parse_file(dat_manager, item.id, map_renderer, hash_index);
									std::wstring savePath = OpenFileDialog(std::format(L"terrain_tex_indices_0x{:X}", item.hash),
										L"png");
									if (!savePath.empty())
									{
										int texture_id = map_renderer->GetTextureManager()->GetTextureIdByHash(item.hash);

										std::wstring filename = std::format(L"texture_0x{:X}.png", item.hash);

										ID3D11ShaderResourceView* texture =
											map_renderer->GetTextureManager()->GetTexture(texture_id);
										if (SaveTextureToPng(texture, savePath, map_renderer->GetTextureManager()))
										{
											// Success
										}
										else
										{
											// Error handling }
										}
									}
								}
							}
							ImGui::EndPopup();
						}

						ImGui::TableNextColumn();

						const auto file_hash_text = std::format("0x{:X} ({})", item.hash, item.hash);
						ImGui::Text(file_hash_text.c_str());
						ImGui::TableNextColumn();

						const auto filename_text = std::format("0x{:X} 0x{:X}", item.file_id_0, item.file_id_1);
						ImGui::Text(filename_text.c_str());
						ImGui::TableNextColumn();


						if (!item.names.empty())
						{
							std::string name;
							for (int i = 0; i < item.names.size(); i++)
							{
								name += item.names[i];
								if (i < item.names.size() - 1) { name += " | "; }
							}

							// Check if the text would be clipped
							ImVec2 textSize = ImGui::CalcTextSize(name.c_str());
							float availableWidth = ImGui::GetContentRegionAvail().x;

							if (textSize.x > availableWidth)
							{
								// Truncate the text to fit the available width
								std::string truncatedName = truncate_text_with_ellipsis(name, availableWidth);

								// Display the truncated text
								ImGui::TextUnformatted(truncatedName.c_str());

								// Check if the mouse is hovering over the text
								if (ImGui::IsItemHovered())
								{
									// Show a tooltip with the full list of names
									ImGui::BeginTooltip();
									ImGui::TextUnformatted(name.c_str());
									ImGui::EndTooltip();
								}
							}
							else
							{
								// Display the full text without truncation
								ImGui::TextUnformatted(name.c_str());
							}
						}
						else { ImGui::Text("-"); }

						ImGui::TableNextColumn();
						ImGui::Text(typeToString(item.type).c_str());
						ImGui::TableNextColumn();
						ImGui::Text("%04d", item.size);
						ImGui::TableNextColumn();
						ImGui::Text("%04d", item.decompressed_size);
						ImGui::TableNextColumn();
						if (item.type == FFNA_Type3)
						{
							std::string map_ids_text;
							for (int i = 0; i < item.map_ids.size(); i++)
							{
								map_ids_text += std::format("{}", item.map_ids[i]);
								if (i < item.map_ids.size() - 1) { map_ids_text += ","; }
							}

							// Check if the text would be clipped
							ImVec2 textSize = ImGui::CalcTextSize(map_ids_text.c_str());
							float availableWidth = ImGui::GetContentRegionAvail().x;

							if (textSize.x > availableWidth)
							{
								// Truncate the text to fit the available width
								std::string truncatedMapIdsText =
									truncate_text_with_ellipsis(map_ids_text, availableWidth);

								// Display the truncated text
								ImGui::TextUnformatted(truncatedMapIdsText.c_str());

								// Check if the mouse is hovering over the text
								if (ImGui::IsItemHovered())
								{
									// Show a tooltip with the full list of map_ids
									ImGui::BeginTooltip();
									ImGui::TextUnformatted(map_ids_text.c_str());
									ImGui::EndTooltip();
								}
							}
							else
							{
								// Display the full text without truncation
								ImGui::TextUnformatted(map_ids_text.c_str());
							}
						}
						else { ImGui::Text("-"); }
						ImGui::TableNextColumn();

						// Display the checkboxes only if there is enough room for all of them.
						// If there is not enough room, display a single checkbox if all checkboxes share the same value (either all true or all false), or display '...' otherwise.
						if (item.type == FFNA_Type3 && item.is_pvp.size() > 0)
						{
							ImVec2 checkboxSize = ImGui::CalcTextSize("[ ]");
							float availableWidth = ImGui::GetContentRegionAvail().x;
							float requiredWidth = checkboxSize.x * item.is_pvp.size() +
								(item.is_pvp.size() - 1) * ImGui::GetStyle().ItemSpacing.x;

							bool allTrue =
								std::all_of(item.is_pvp.begin(), item.is_pvp.end(), [](int v) { return v >= 1; });
							bool allFalse =
								std::all_of(item.is_pvp.begin(), item.is_pvp.end(), [](int v) { return v < 1; });

							if (allTrue || allFalse)
							{
								ImGui::BeginDisabled(); // Disable the checkbox to make it non-editable
								ImGui::Checkbox("##IsPvp",
									&allTrue); // Show a single checkbox with the respective value
								ImGui::EndDisabled();
							}
							else if (requiredWidth > availableWidth)
							{
								ImGui::TextUnformatted("..."); // Not enough space and mixed values, show '...'
							}
							else
							{
								for (int i = 0; i < item.is_pvp.size(); i++)
								{
									ImGui::PushID(i);
									ImGui::BeginDisabled(); // Disable the checkbox to make it non-editable
									ImGui::Checkbox(("##IsPvp" + std::to_string(i)).c_str(), (bool*)&item.is_pvp[i]);
									ImGui::EndDisabled();
									ImGui::PopID();

									if (i < item.is_pvp.size() - 1) { ImGui::SameLine(); }
								}
							}
						}
						else { ImGui::Text("-"); }

						ImGui::TableNextColumn();
						ImGui::Text("%u", item.murmurhash3);

						ImGui::TableNextColumn();
						// Render chunk IDs as comma-separated hex values
						if (!item.chunk_ids.empty())
						{
							std::string chunks_text;
							for (size_t i = 0; i < item.chunk_ids.size(); i++)
							{
								chunks_text += std::format("0x{:X}", item.chunk_ids[i]);
								if (i < item.chunk_ids.size() - 1) { chunks_text += ", "; }
							}

							// Check if the text would be clipped
							ImVec2 textSize = ImGui::CalcTextSize(chunks_text.c_str());
							float availableWidth = ImGui::GetContentRegionAvail().x;

							if (textSize.x > availableWidth)
							{
								// Truncate the text to fit the available width
								std::string truncatedChunksText = truncate_text_with_ellipsis(chunks_text, availableWidth);

								// Display the truncated text
								ImGui::TextUnformatted(truncatedChunksText.c_str());

								// Check if the mouse is hovering over the text
								if (ImGui::IsItemHovered())
								{
									// Show a tooltip with the full list of chunk IDs
									ImGui::BeginTooltip();
									ImGui::TextUnformatted(chunks_text.c_str());
									ImGui::EndTooltip();
								}
							}
							else
							{
								// Display the full text without truncation
								ImGui::TextUnformatted(chunks_text.c_str());
							}
						}
						else { ImGui::Text("-"); }

						ImGui::PopID();
					}
				ImGui::EndTable();
			}
		}

		ImGui::End();
	}
}

void ExportDDS2(DATManager* dat_manager, DatBrowserItem& item, MapRenderer* map_renderer, std::unordered_map<int, std::vector<int>>& hash_index, const CompressionFormat& compression_format)
{
	parse_file(dat_manager, item.id, map_renderer, hash_index);
	std::wstring savePath = OpenFileDialog(std::format(L"texture_0x{:X}", item.hash),
		L"dds");
	if (!savePath.empty())
	{
		const auto texture_data = map_renderer->GetTextureManager()->GetTextureDataByHash(item.hash);
		if (texture_data.has_value()) {
			std::wstring filename = std::format(L"texture_0x{:X}.dds", item.hash);

			if (SaveTextureToDDS(texture_data.value(), savePath, compression_format))
			{
				// Success
			}
			else
			{
				// Error handling }
			}
		}
	}
}

void ExportDDS(DATManager* dat_manager, int mft_file_index, int file_id, MapRenderer* map_renderer, std::unordered_map<int, std::vector<int>>& hash_index, const CompressionFormat& compression_format)
{
	std::wstring saveDir = OpenDirectoryDialog();
	if (!saveDir.empty())
	{
		parse_file(dat_manager, mft_file_index, map_renderer, hash_index);

		for (int tex_index = 0; tex_index < selected_ffna_model_file.texture_filenames_chunk.
			texture_filenames.
			size(); tex_index++)
		{
			const auto& texture_filename = selected_ffna_model_file.texture_filenames_chunk.
				texture_filenames[tex_index];

			auto decoded_filename = decode_filename(texture_filename.id0, texture_filename.id1);
			const auto texture_data = map_renderer->GetTextureManager()->GetTextureDataByHash(decoded_filename);

			if (texture_data.has_value()) {

				std::wstring filename = std::format(L"model_0x{:X}_tex_index{}_texture_0x{:X}.dds",
					file_id, tex_index, decoded_filename);

				// Append the filename to the saveDir
				std::wstring savePath = saveDir + L"\\" + filename;

				if (SaveTextureToDDS(texture_data.value(), savePath, compression_format))
				{
					// Success
				}
				else
				{
					// Error handling }
				}
			}
		}
	}
}

inline int IMGUI_CDECL DatBrowserItem::CompareWithSortSpecs(const void* lhs, const void* rhs)
{
	auto a = static_cast<const DatBrowserItem*>(lhs);
	auto b = static_cast<const DatBrowserItem*>(rhs);
	for (int n = 0; n < s_current_sort_specs->SpecsCount; n++)
	{
		const ImGuiTableColumnSortSpecs* sort_spec = &s_current_sort_specs->Specs[n];
		int delta = 0;
		switch (sort_spec->ColumnUserID)
		{
		case DatBrowserItemColumnID_id:
			delta = (a->id - b->id);
			break;
		case DatBrowserItemColumnID_hash:
			delta = (a->hash - b->hash);
			break;
		case DatBrowserItemColumnID_murmurhash3:
			delta = (a->murmurhash3 - b->murmurhash3);
			break;
		case DatBrowserItemColumnID_type:
			delta = (a->type - b->type);
			break;
		case DatBrowserItemColumnID_size:
			delta = (a->size - b->size);
			break;
		case DatBrowserItemColumnID_decompressed_size:
			delta = (a->decompressed_size - b->decompressed_size);
			break;
		case DatBrowserItemColumnID_filename:
			delta = (a->file_id_0 + (a->file_id_1 << 16)) - (b->file_id_0 + (b->file_id_1 << 16));
			break;
		case DatBrowserItemColumnID_map_id: // Modified map_id case
			for (size_t i = 0; i < std::min(a->map_ids.size(), b->map_ids.size()); ++i)
			{
				delta = (a->map_ids[i] - b->map_ids[i]);
				if (delta != 0)
					break;
			}
			if (delta == 0)
				delta = (a->map_ids.size() - b->map_ids.size());
			break;

		case DatBrowserItemColumnID_name: // Modified name case
			for (size_t i = 0; i < std::min(a->names.size(), b->names.size()); ++i)
			{
				delta = a->names[i].compare(b->names[i]);
				if (delta != 0)
					break;
			}
			if (delta == 0)
				delta = (a->names.size() - b->names.size());
			break;

		case DatBrowserItemColumnID_is_pvp: // Modified is_pvp case
			for (size_t i = 0; i < std::min(a->is_pvp.size(), b->is_pvp.size()); ++i)
			{
				delta = (a->is_pvp[i] - b->is_pvp[i]);
				if (delta != 0)
					break;
			}
			if (delta == 0)
				delta = (a->is_pvp.size() - b->is_pvp.size());
			break;
		case DatBrowserItemColumnID_chunk_ids:
			// Compare by first chunk ID, or by count if first IDs are equal
			{
				uint32_t a_first = a->chunk_ids.empty() ? 0 : a->chunk_ids[0];
				uint32_t b_first = b->chunk_ids.empty() ? 0 : b->chunk_ids[0];
				if (a_first != b_first)
					delta = (a_first > b_first) ? 1 : -1;
				else
					delta = static_cast<int>(a->chunk_ids.size()) - static_cast<int>(b->chunk_ids.size());
			}
			break;
		default:
			IM_ASSERT(0);
			break;
		}
		if (delta > 0)
			return (sort_spec->SortDirection == ImGuiSortDirection_Ascending) ? +1 : -1;
		if (delta < 0)
			return (sort_spec->SortDirection == ImGuiSortDirection_Ascending) ? -1 : +1;
	}

	return (a->id - b->id);
}

std::string truncate_text_with_ellipsis(const std::string& text, float maxWidth)
{
	ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
	if (textSize.x <= maxWidth) { return text; }

	std::string truncatedText;
	float ellipsisWidth = ImGui::CalcTextSize("...").x;

	for (size_t i = 0; i < text.size(); ++i)
	{
		textSize = ImGui::CalcTextSize((truncatedText + text[i] + "...").c_str());
		if (textSize.x <= maxWidth) { truncatedText += text[i]; }
		else { break; }
	}

	truncatedText += "...";
	return truncatedText;
}

int custom_stoi(const std::string& input)
{
	const char* str = input.c_str();
	int value = 0;
	bool negative = false;

	// skip leading whitespace
	while (*str == ' ' || *str == '\t' || *str == '\r' || *str == '\n') { ++str; }

	// check for sign
	if (*str == '-')
	{
		negative = true;
		++str;
	}
	else if (*str == '+') { ++str; }

	// check for hex prefix
	if (std::strlen(str) >= 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
	{
		str += 2;

		// read hex digits
		while (*str != '\0')
		{
			if (*str >= '0' && *str <= '9') { value = value * 16 + (*str - '0'); }
			else if (*str >= 'a' && *str <= 'f') { value = value * 16 + (*str - 'a' + 10); }
			else if (*str >= 'A' && *str <= 'F') { value = value * 16 + (*str - 'A' + 10); }
			else
			{
				return -1; // invalid character
			}

			++str;
		}
	}
	else
	{
		// read decimal digits
		while (*str != '\0')
		{
			if (*str >= '0' && *str <= '9') { value = value * 10 + (*str - '0'); }
			else
			{
				return -1; // invalid character
			}

			++str;
		}
	}

	return negative ? -value : value;
}

std::string to_lower(const std::string& input)
{
	std::string result = input;
	std::transform(result.begin(), result.end(), result.begin(),
		[](unsigned char c) { return std::tolower(c); });
	return result;
}

DirectX::XMFLOAT4 GetAverageColorOfBottomRow(const DatTexture& dat_texture) {
	int total_pixels = dat_texture.width;

	unsigned long long total_r = 0, total_g = 0, total_b = 0;

	// Iterate over the bottom row of pixels
	for (int x = 0; x < dat_texture.width; ++x) {
		int index = (dat_texture.height - 1) * dat_texture.width + x;
		RGBA pixel = dat_texture.rgba_data[index];

		total_r += pixel.r;
		total_g += pixel.g;
		total_b += pixel.b;
	}

	// Calculate average color
	float avg_r = static_cast<float>(total_r) / total_pixels / 255.0f;
	float avg_g = static_cast<float>(total_g) / total_pixels / 255.0f;
	float avg_b = static_cast<float>(total_b) / total_pixels / 255.0f;

	// Return average color as XMFLOAT4
	return DirectX::XMFLOAT4(avg_b, avg_g, avg_r, 1.0f); // Alpha set to 1.0f (fully opaque)
}

GWVertex get_shore_vertex_for_2_points(Vertex2 point1, Vertex2 point2, float height) {
	XMFLOAT2 diff_vec{ point2.x - point1.x, point2.y - point1.y };
	// Calculate the length of diff_vec for normalization
	float length = sqrt(diff_vec.x * diff_vec.x + diff_vec.y * diff_vec.y);

	// Normalize diff_vec
	XMFLOAT2 normalized_diff_vec{ diff_vec.x / length, diff_vec.y / length };

	// Apply shore_offset
	float shore_offset = 220.0f; // The offset distance for the shore vertices
	XMFLOAT2 offset_vec{ normalized_diff_vec.y * shore_offset, -normalized_diff_vec.x * shore_offset };

	// Calculate heights at offset points
	float height0 = terrain->get_height_at(point1.x - offset_vec.x, point1.y - offset_vec.y);
	float height1 = terrain->get_height_at(point2.x + offset_vec.x, point2.y + offset_vec.y);

	if (height0 < height1) {
		return GWVertex({ point1.x - offset_vec.x, height, point1.y - offset_vec.y }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f });
	}
	else {
		return GWVertex({ point1.x + offset_vec.x, height, point1.y + offset_vec.y }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f });
	}
}

GWVertex get_shore_vertex_for_3_points(Vertex2 point1, Vertex2 point2, Vertex2 point3, float height) {
	XMFLOAT2 diff_vec1{ point2.x - point1.x, point2.y - point1.y };
	XMFLOAT2 diff_vec2{ point3.x - point2.x, point3.y - point2.y };

	// Interpolate vectors to find avg direction of the shore.
	XMFLOAT2 diff_vec{ (diff_vec1.x + diff_vec2.x) / 2.0f, (diff_vec1.y + diff_vec2.y) / 2.0f };

	// Calculate the length of diff_vec for normalization
	float length = sqrt(diff_vec.x * diff_vec.x + diff_vec.y * diff_vec.y);

	// Normalize diff_vec
	XMFLOAT2 normalized_diff_vec{ diff_vec.x / length, diff_vec.y / length };

	// Apply shore_offset
	float shore_offset = 220.0f; // The offset distance for the shore vertices
	XMFLOAT2 offset_vec{ normalized_diff_vec.y * shore_offset, -normalized_diff_vec.x * shore_offset };

	// Calculate heights at offset points. I.e. we rotate the vector 90 degrees in either direction to find where the water is.
	// Whichever point is lower contains the water. It is possible that both sides are in water so we might wanna check that the
	// heights are below water level rather than compare them but this will do unless that becomes an issue.
	float height0 = terrain->get_height_at(point1.x - offset_vec.x, point1.y - offset_vec.y);
	float height1 = terrain->get_height_at(point2.x + offset_vec.x, point2.y + offset_vec.y);

	if (height0 < height1) {
		return GWVertex({ point2.x - offset_vec.x, height, point2.y - offset_vec.y }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f });
	}
	else {
		return GWVertex({ point2.x + offset_vec.x, height, point2.y + offset_vec.y }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f });
	}
}

void generate_shore_mesh(const XMFLOAT2& point1, const XMFLOAT2& point2, Terrain* terrain, std::vector<Mesh>& meshes, float height) {
	XMFLOAT2 diff_vec{ point2.x - point1.x, point2.y - point1.y };
	// Calculate the length of diff_vec for normalization
	float length = sqrt(diff_vec.x * diff_vec.x + diff_vec.y * diff_vec.y);

	// Normalize diff_vec
	XMFLOAT2 normalized_diff_vec{ diff_vec.x / length, diff_vec.y / length };

	// Apply shore_offset
	float shore_offset = 220.0f; // The offset distance for the shore vertices
	XMFLOAT2 offset_vec{ normalized_diff_vec.y * shore_offset, -normalized_diff_vec.x * shore_offset };

	// Calculate heights at offset points
	float height0 = terrain->get_height_at(point1.x - offset_vec.x, point1.y - offset_vec.y);
	float height1 = terrain->get_height_at(point2.x + offset_vec.x, point2.y + offset_vec.y);

	Mesh shore_mesh;

	// Determine the direction of the shore mesh based on the terrain height
	if (height0 < height1) {
		// Extend the mesh outwards from point1 and point2 in the counter-clockwise direction
		shore_mesh.vertices.emplace_back(GWVertex({ point1.x, height, point1.y }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f }));
		shore_mesh.vertices.emplace_back(GWVertex({ point2.x, height, point2.y }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f }));
		shore_mesh.vertices.emplace_back(GWVertex({ point1.x - offset_vec.x, height, point1.y - offset_vec.y }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f }));
		shore_mesh.vertices.emplace_back(GWVertex({ point2.x - offset_vec.x, height, point2.y - offset_vec.y }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f }));
	}
	else {
		// Extend the mesh outwards from point1 and point2 in the clockwise direction
		shore_mesh.vertices.emplace_back(GWVertex({ point1.x, height, point1.y }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f }));
		shore_mesh.vertices.emplace_back(GWVertex({ point2.x, height, point2.y }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f }));
		shore_mesh.vertices.emplace_back(GWVertex({ point1.x + offset_vec.x, height, point1.y + offset_vec.y }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f }));
		shore_mesh.vertices.emplace_back(GWVertex({ point2.x + offset_vec.x, height, point2.y + offset_vec.y }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f }));
	}

	// Define indices for the two triangles that make up the rectangular section of the shore mesh
	shore_mesh.indices.insert(shore_mesh.indices.end(), { 0, 2, 3, 0, 3, 1 });

	meshes.push_back(shore_mesh);
}
