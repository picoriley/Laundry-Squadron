#include "Engine/Renderer/Mesh.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/Vertex.hpp"
#include "Engine/Math/Vector3Int.hpp"
#include "Engine/Math/Vector3.hpp"
#include "Engine/Math/Vector2.hpp"
#include "Engine/Renderer/ShaderProgram.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/Material.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <gl/GL.h>
#include <gl/GLU.h>
#include "Engine/Renderer/OpenGLExtensions.hpp"

//-----------------------------------------------------------------------------------
Mesh::Mesh()
{

}

//-----------------------------------------------------------------------------------
Mesh::~Mesh()
{

}

//-----------------------------------------------------------------------------------
Mesh Mesh::CreateCube(float sideLength, const RGBA& color /*= RGBA::WHITE*/)
{
	const float halfSideLength = sideLength / 2.0f;
	Mesh meshToCreate;

	//Add all of the vertices to the vector of verts.
	//Vector3 points[] = Vector3[8];
	for (int i = 0; i < 8; i++)
	{
		Vertex_PCUTB vert;
		vert.pos = Vector3(((i & 0x01) == 0) ? halfSideLength : -halfSideLength,
			((i & 0x02) == 0) ? halfSideLength : -halfSideLength,
			((i & 0x04) == 0) ? halfSideLength : -halfSideLength);
		vert.color = color;
		vert.texCoords = Vector2(((i & 0x01) == 0) ? 0.0f : 1.0f,
			((i & 0x02) == 0) ? 0.0f : 1.0f);

		meshToCreate.m_verts.push_back(vert);
	}

// 	for (int i = 0; i < 24; i++)
// 	{
// 		Vertex_PCT vert;
// 		vert.pos = points[i % 3];
// 		vert.color = color;
// 		vert.texCoords = Vector2(((i & 0x01) == 0) ? 0.0f : 1.0f,
// 								 ((i & 0x02) == 0) ? 0.0f : 1.0f); 
// 
// 		meshToCreate.m_verts.push_back(vert);													
// 	}												

	//Set up the cube faces.
	meshToCreate.m_indices.push_back(0); meshToCreate.m_indices.push_back(2); meshToCreate.m_indices.push_back(1);
	meshToCreate.m_indices.push_back(1); meshToCreate.m_indices.push_back(2); meshToCreate.m_indices.push_back(3);
	meshToCreate.m_indices.push_back(0); meshToCreate.m_indices.push_back(6); meshToCreate.m_indices.push_back(2);
	meshToCreate.m_indices.push_back(0); meshToCreate.m_indices.push_back(4); meshToCreate.m_indices.push_back(6);
	meshToCreate.m_indices.push_back(4); meshToCreate.m_indices.push_back(5); meshToCreate.m_indices.push_back(0);
	meshToCreate.m_indices.push_back(5); meshToCreate.m_indices.push_back(1); meshToCreate.m_indices.push_back(0);
	meshToCreate.m_indices.push_back(4); meshToCreate.m_indices.push_back(5); meshToCreate.m_indices.push_back(6);
	meshToCreate.m_indices.push_back(5); meshToCreate.m_indices.push_back(7); meshToCreate.m_indices.push_back(6);
	meshToCreate.m_indices.push_back(5); meshToCreate.m_indices.push_back(3); meshToCreate.m_indices.push_back(7);
	meshToCreate.m_indices.push_back(1); meshToCreate.m_indices.push_back(3); meshToCreate.m_indices.push_back(5);
	meshToCreate.m_indices.push_back(6); meshToCreate.m_indices.push_back(2); meshToCreate.m_indices.push_back(7);
	meshToCreate.m_indices.push_back(7); meshToCreate.m_indices.push_back(2); meshToCreate.m_indices.push_back(3);
	
	meshToCreate.Init();
	return meshToCreate;
}

Mesh Mesh::CreateIcoSphere(float radius, const RGBA& color /*= RGBA::WHITE*/, int numPasses /*= 3*/)
{
	Mesh meshToCreate;
	Vector3 initialPoints[6] = { { 0, 0, radius },{ 0, 0, -radius },{ -radius, -radius, 0 },{ radius, -radius, 0 },{ radius, radius, 0 },{ -radius,  radius, 0 } };
	Vector2 initialUVs[6] = { {0.5f, 0.5f}, {0.5f, 0.5f}, {1.0f, 1.0f}, {0.0f, 1.0f}, { 0.0f, 0.0f },{ 1.0f, 0.0f } };
	Vertex_PCUTB currentWorkingVertex;
	currentWorkingVertex.color = color;

	for (int i = 0; i < 6; i++)
	{
		currentWorkingVertex.pos = Vector3::GetNormalized(initialPoints[i]) * radius;
		currentWorkingVertex.texCoords = initialUVs[i];
		meshToCreate.m_verts.push_back(currentWorkingVertex);
	}

	meshToCreate.m_indices.push_back(0); meshToCreate.m_indices.push_back(3); meshToCreate.m_indices.push_back(4);
	meshToCreate.m_indices.push_back(0); meshToCreate.m_indices.push_back(4); meshToCreate.m_indices.push_back(5);
	meshToCreate.m_indices.push_back(0); meshToCreate.m_indices.push_back(5); meshToCreate.m_indices.push_back(2);
	meshToCreate.m_indices.push_back(0); meshToCreate.m_indices.push_back(2); meshToCreate.m_indices.push_back(3);
	meshToCreate.m_indices.push_back(1); meshToCreate.m_indices.push_back(4); meshToCreate.m_indices.push_back(3);
	meshToCreate.m_indices.push_back(1); meshToCreate.m_indices.push_back(5); meshToCreate.m_indices.push_back(4);
	meshToCreate.m_indices.push_back(1); meshToCreate.m_indices.push_back(2); meshToCreate.m_indices.push_back(5);
	meshToCreate.m_indices.push_back(1); meshToCreate.m_indices.push_back(3); meshToCreate.m_indices.push_back(2);

	for (int i = 0; i < numPasses; i++)
	{
		int numberOfFaces = meshToCreate.m_indices.size() / 3;
		int indicesIndex = 0;
		for (int j = 0; j < numberOfFaces; j++)
		{
			//Get first 3 indices
			const int x = indicesIndex++;
			const int y = indicesIndex++;
			const int z = indicesIndex++;
			// Calculate the midpoints 
			Vector3 point1 = Vector3::GetMidpoint(meshToCreate.m_verts.at(meshToCreate.m_indices.at(x)).pos, meshToCreate.m_verts.at(meshToCreate.m_indices.at(y)).pos);
			Vector3 point2 = Vector3::GetMidpoint(meshToCreate.m_verts.at(meshToCreate.m_indices.at(y)).pos, meshToCreate.m_verts.at(meshToCreate.m_indices.at(z)).pos);
			Vector3 point3 = Vector3::GetMidpoint(meshToCreate.m_verts.at(meshToCreate.m_indices.at(z)).pos, meshToCreate.m_verts.at(meshToCreate.m_indices.at(x)).pos);

			//Calculate UV midpoints
			Vector2 uvPoint1 = Vector2::GetMidpoint(meshToCreate.m_verts.at(meshToCreate.m_indices.at(x)).texCoords, meshToCreate.m_verts.at(meshToCreate.m_indices.at(y)).texCoords);
			Vector2 uvPoint2 = Vector2::GetMidpoint(meshToCreate.m_verts.at(meshToCreate.m_indices.at(y)).texCoords, meshToCreate.m_verts.at(meshToCreate.m_indices.at(z)).texCoords);
			Vector2 uvPoint3 = Vector2::GetMidpoint(meshToCreate.m_verts.at(meshToCreate.m_indices.at(z)).texCoords, meshToCreate.m_verts.at(meshToCreate.m_indices.at(x)).texCoords);

			//Move the points to the outside of our sphere.
			point1.Normalize();
			point2.Normalize();
			point3.Normalize();

			//Add these vertices to the list of verts, and store their array location.
			//Naiive way to check for dupes is here. I'll find a better solution later.
			int point1Location = -1;
			int point2Location = -1;
			int point3Location = -1;
			for (unsigned int q = 0; q < meshToCreate.m_verts.size(); q++)
			{
				Vector3 compare = meshToCreate.m_verts.at(q).pos;
				if (compare == point1)
					point1Location = q;
				if (compare == point2)
					point2Location = q;
				if (compare == point3)
					point3Location = q;
			}
			if (point1Location == -1)
			{
				point1Location = meshToCreate.m_verts.size();
				currentWorkingVertex.pos = point1 * radius;
				currentWorkingVertex.texCoords = uvPoint1;
				meshToCreate.m_verts.push_back(currentWorkingVertex);
			}

			if (point2Location == -1)
			{
				point2Location = meshToCreate.m_verts.size();
				currentWorkingVertex.pos = point2 * radius;
				currentWorkingVertex.texCoords = uvPoint2;
				meshToCreate.m_verts.push_back(currentWorkingVertex);
			}

			if (point3Location == -1)
			{
				point3Location = meshToCreate.m_verts.size();
				currentWorkingVertex.pos = point3 * radius;
				currentWorkingVertex.texCoords = uvPoint3;
				meshToCreate.m_verts.push_back(currentWorkingVertex);
			}

			//Create 3 new faces (the outer triangles, the pieces of the triforce)
			meshToCreate.m_indices.push_back(meshToCreate.m_indices.at(x));	meshToCreate.m_indices.push_back(point1Location);				meshToCreate.m_indices.push_back(point3Location);
			meshToCreate.m_indices.push_back(point1Location);				meshToCreate.m_indices.push_back(meshToCreate.m_indices.at(y));	meshToCreate.m_indices.push_back(point2Location);
			meshToCreate.m_indices.push_back(point3Location);				meshToCreate.m_indices.push_back(point2Location);				meshToCreate.m_indices.push_back(meshToCreate.m_indices.at(z));

			//Replace the original face with the inner, upside-down triangle (not the triforce)
			meshToCreate.m_indices.at(x) = point1Location;
			meshToCreate.m_indices.at(y) = point2Location;
			meshToCreate.m_indices.at(z) = point3Location;
		}
	}

	meshToCreate.Init();
	return meshToCreate;
}

Mesh Mesh::CreateQuad(const Vector3& bottomLeft, const Vector3& topRight, const RGBA& color /*= RGBA::WHITE*/)
{
	Mesh meshToCreate;

#pragma FIXME("This only works in 2D")
	meshToCreate.m_verts.push_back(Vertex_PCUTB(bottomLeft, color, Vector2(0.0f, 1.0f), Vector3::UNIT_X, Vector3::UNIT_Y));
	meshToCreate.m_verts.push_back(Vertex_PCUTB(Vector3(bottomLeft.x, topRight.y, topRight.z), color, Vector2(0.0f, 0.0f), Vector3::UNIT_X, Vector3::UNIT_Y));
	meshToCreate.m_verts.push_back(Vertex_PCUTB(Vector3(topRight.x, bottomLeft.y, bottomLeft.z), color, Vector2(1.0f, 1.0f), Vector3::UNIT_X, Vector3::UNIT_Y));
	meshToCreate.m_verts.push_back(Vertex_PCUTB(topRight, color, Vector2(1.0f, 0.0f), Vector3::UNIT_X, Vector3::UNIT_Y));

	meshToCreate.m_indices.push_back(0);
	meshToCreate.m_indices.push_back(1);
	meshToCreate.m_indices.push_back(2);
	meshToCreate.m_indices.push_back(1);
	meshToCreate.m_indices.push_back(3);
	meshToCreate.m_indices.push_back(2);

	meshToCreate.Init();

	return meshToCreate;
}

void Mesh::Init()
{
	m_vbo = Renderer::instance->GenerateBufferID();
	GL_CHECK_ERROR();
	Renderer::instance->BindAndBufferVBOData(m_vbo, m_verts.data(), m_verts.size());
	GL_CHECK_ERROR();
	m_ibo = Renderer::instance->RenderBufferCreate(m_indices.data(), m_indices.size(), sizeof(unsigned int), GL_STATIC_DRAW);
	GL_CHECK_ERROR();
}

//-----------------------------------------------------------------------------------
void Mesh::RenderFromIBO(GLuint vaoID, const Material& material) const
{
	glBindVertexArray(vaoID);
	material.SetUpRenderState();
	//Draw with IBO
	glDrawElements(GL_TRIANGLES, m_indices.size(), GL_UNSIGNED_INT, (GLvoid*)0);
	glUseProgram(NULL);
	glBindVertexArray(NULL);
	material.CleanUpRenderState();
}
