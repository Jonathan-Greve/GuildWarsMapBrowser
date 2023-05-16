#pragma once
#include "Mesh.h"
#include <sstream>

inline std::string write_obj_str(const Mesh* mesh)
{
    std::stringstream objStream;

    // Write vertices and vertex normals to the string stream
    for (const auto& vertex : mesh->vertices) {
        objStream << "v " << vertex.position.x << " " << vertex.position.y << " " << -vertex.position.z << "\n";
        objStream << "vn " << vertex.normal.x << " " << vertex.normal.y << " " << -vertex.normal.z << "\n";
        objStream << "vt " << vertex.tex_coord0.x << " " << vertex.tex_coord0.y << "\n";
    }

    // Write faces (triangles) to the string stream
    for (size_t i = 0; i < mesh->indices.size(); i += 3) {
        objStream << "f "
            << (mesh->indices[i] + 1) << "/" << (mesh->indices[i] + 1) << "/" << (mesh->indices[i] + 1) << " "
            << (mesh->indices[i + 1] + 1) << "/" << (mesh->indices[i + 1] + 1) << "/" << (mesh->indices[i + 1] + 1) << " "
            << (mesh->indices[i + 2] + 1) << "/" << (mesh->indices[i + 2] + 1) << "/" << (mesh->indices[i + 2] + 1) << "\n";
    }

    return objStream.str();

}
