#include "g_mesh.h"
#include <tiny_obj_loader.h>
#include <iostream>
#include <filesystem>

namespace zebra {
	bool Mesh::load_from_obj(const char* file) {
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;

		std::string warn;
		std::string err;

		tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, file, nullptr);

		if (!warn.empty()) {
			std::cout << "WARN: " << warn << std::endl;
		}

		if (!err.empty()) {
			std::cerr << err << std::endl;
			return false;
		}

		for (size_t s = 0; s < shapes.size(); s++) {
			size_t index_offset = 0;
			for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
				//hardcode loading to triangles
				int fv = 3;

				// Loop over vertices in the face.
				for (size_t v = 0; v < fv; v++) {
					// access to vertex
					tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

					//vertex position
					tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
					tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
					tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
					//vertex normal
					tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
					tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
					tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];

					//copy it into our vertex
					P3N3C3 new_vert;
					new_vert.pos.x = vx;
					new_vert.pos.y = vy;
					new_vert.pos.z = vz;

					new_vert.normal.x = nx;
					new_vert.normal.y = ny;
					new_vert.normal.z = nz;

					//we are setting the vertex color as the vertex normal. This is just for display purposes
					new_vert.color = new_vert.normal;

					_vertices.push_back(new_vert);
				}
				index_offset += fv;
			}
		}
		return true;
	}
}