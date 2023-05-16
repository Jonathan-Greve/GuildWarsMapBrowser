#pragma once
#include "Mesh.h"
#include <sstream>

inline std::string write_obj_str(const Mesh* mesh)
{
    std::stringstream objStream;

    // Write vertices and vertex normals to the string stream
    for (const auto& vertex : mesh->vertices)
    {
        objStream << "v " << vertex.position.x << " " << vertex.position.y << " " << -vertex.position.z
                  << "\n";
        objStream << "vn " << vertex.normal.x << " " << vertex.normal.y << " " << -vertex.normal.z << "\n";
        objStream << "vt " << vertex.tex_coord0.x << " " << vertex.tex_coord0.y << "\n";
    }

    // Write faces (triangles) to the string stream
    for (size_t i = 0; i < mesh->indices.size(); i += 3)
    {
        objStream << "f " << (mesh->indices[i] + 1) << "/" << (mesh->indices[i] + 1) << "/"
                  << (mesh->indices[i] + 1) << " " << (mesh->indices[i + 1] + 1) << "/"
                  << (mesh->indices[i + 1] + 1) << "/" << (mesh->indices[i + 1] + 1) << " "
                  << (mesh->indices[i + 2] + 1) << "/" << (mesh->indices[i + 2] + 1) << "/"
                  << (mesh->indices[i + 2] + 1) << "\n";
    }

    return objStream.str();
}

inline std::string write_obj_str(const std::vector<Mesh>& meshes)
{
    std::stringstream objStream;

    size_t vertex_offset = 0;
    size_t texture_offset = 0;
    size_t normal_offset = 0;

    for (const Mesh& mesh : meshes)
    {
        // Write vertices, normals and texture coordinates to the string stream
        for (const auto& vertex : mesh.vertices)
        {
            objStream << "v " << vertex.position.x << " " << vertex.position.y << " " << -vertex.position.z
                      << "\n";
            objStream << "vn " << vertex.normal.x << " " << vertex.normal.y << " " << -vertex.normal.z
                      << "\n";
            objStream << "vt " << vertex.tex_coord0.x << " " << vertex.tex_coord0.y << "\n";
        }

        // Write faces (triangles) to the string stream, offsetting the indices
        for (size_t i = 0; i < mesh.indices.size(); i += 3)
        {
            objStream << "f " << (mesh.indices[i] + 1 + vertex_offset) << "/"
                      << (mesh.indices[i] + 1 + texture_offset) << "/"
                      << (mesh.indices[i] + 1 + normal_offset) << " "
                      << (mesh.indices[i + 1] + 1 + vertex_offset) << "/"
                      << (mesh.indices[i + 1] + 1 + texture_offset) << "/"
                      << (mesh.indices[i + 1] + 1 + normal_offset) << " "
                      << (mesh.indices[i + 2] + 1 + vertex_offset) << "/"
                      << (mesh.indices[i + 2] + 1 + texture_offset) << "/"
                      << (mesh.indices[i + 2] + 1 + normal_offset) << "\n";
        }

        // Update the offsets for the next mesh
        vertex_offset += mesh.vertices.size();
        texture_offset += mesh.vertices.size(); // Assuming same number of texture coordinates as vertices
        normal_offset += mesh.vertices.size(); // Assuming same number of normals as vertices
    }

    return objStream.str();
}
