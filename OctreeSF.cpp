
#include <stack>
#include "OctreeSF.h"
#include "SignedDistanceField.h"
#include "MarchingCubes.h"
#include "Mesh.h"
#include <bitset>

/******************************************************************************************
Node constructors
*******************************************************************************************/

OctreeSF::InnerNode::InnerNode(const Area& area, const SignedDistanceField3D& implicitSDF, Vector3iHashGrid<SharedSurfaceVertex*>* sharedVertices)
{
	m_NodeType = INNER;
	/*for (int i = 0; i < 8; i++)
	{
	m_CornerSamples[i] = cornerSamples[i];
	m_CornerSamples[i]->sample->useCount++;
	}*/

	Area subAreas[8];
	area.getSubAreas(subAreas);
	for (int i = 0; i < 8; i++)
	{
		m_Children[i] = OctreeSF::createNode(subAreas[i], implicitSDF, sharedVertices);
	}
}

OctreeSF::InnerNode::~InnerNode()
{
	for (int i = 0; i < 8; i++)
	{
		// m_CornerSamples[i]->sample->useCount--;
		delete m_Children[i];
	}
}

OctreeSF::InnerNode::InnerNode(const InnerNode& rhs)
{
	m_NodeType = INNER;
	for (int i = 0; i < 8; i++)
	{
		m_Children[i] = rhs.m_Children[i]->clone();
	}
}

OctreeSF::EmptyNode::EmptyNode(const Area& area, const SignedDistanceField3D& implicitSDF)
{
	m_NodeType = EMPTY;
	m_Sign = implicitSDF.getSign(area.toAABB().getCenter());
}

OctreeSF::EmptyNode::~EmptyNode()
{
}

// dist1 * w + dist2 * (1-w) = 0
// => w  = dist2 / (dist2 - dist1)
static float getInterpolationWeight(float dist1, float dist2)
{
	return dist2 / (dist2 - dist1);
}

OctreeSF::GridNode::GridNode(const Area& area, const SignedDistanceField3D& implicitSDF, Vector3iHashGrid<SharedSurfaceVertex*>* sharedVertices)
{
	m_NodeType = GRID;
	float stepSize = area.m_RealSize / LEAF_SIZE_1D_INNER;

	// compute inner grid
	Ogre::Vector3 currentPos = area.m_MinRealPos;
	int index = 0;
	for (unsigned int x = 0; x < LEAF_SIZE_1D; x++)
	{
		for (unsigned int y = 0; y < LEAF_SIZE_1D; y++)
		{
			for (unsigned int z = 0; z < LEAF_SIZE_1D; z++)
			{
				m_Signs[index++] = implicitSDF.getSign(currentPos);
				currentPos.z += stepSize;
			}
			currentPos.z = area.m_MinRealPos.z;
			currentPos.y += stepSize;
		}
		currentPos.y = area.m_MinRealPos.y;
		currentPos.x += stepSize;
	}

	Sample s1, s2;
	currentPos = area.m_MinRealPos;
	for (unsigned int x = 0; x < LEAF_SIZE_1D; x++)
	{
		for (unsigned int y = 0; y < LEAF_SIZE_1D; y++)
		{
			for (unsigned int z = 0; z < LEAF_SIZE_1D; z++)
			{
				index = indexOf(x, y, z);
				Vector3i iPos(x, y, z);
				if (x < LEAF_SIZE_1D_INNER && m_Signs[index] != m_Signs[indexOf(x + 1, y, z)])
				{
					/*implicitSDF.getSample(currentPos, s1);
					implicitSDF.getSample(currentPos + Ogre::Vector3(stepSize, 0, 0), s2);
					vAssert(!signsAreEqual(s1.signedDistance, s2.signedDistance));
					float w = getInterpolationWeight(s1.signedDistance, s2.signedDistance);
					Ogre::Vector3 intersectionPos = MathMisc::linearInterpolation(currentPos, currentPos + Ogre::Vector3(stepSize, 0, 0), w);*/
					m_SurfaceEdges.emplace_back(area.m_MinPos, iPos, 0, currentPos + Ogre::Vector3(stepSize * 0.5f, 0, 0), implicitSDF, sharedVertices);
					// m_SurfaceEdges.emplace_back(index, indexOf(x + 1, y, z), currentPos);
					// m_SurfaceEdges.back().surfacePos.x = MathMisc::linearInterpolation(currentPos.x, currentPos.x + stepSize, getInterpolationWeight(s1.signedDistance, s2.signedDistance));
				}
				if (y < LEAF_SIZE_1D_INNER && m_Signs[index] != m_Signs[indexOf(x, y + 1, z)])
				{
					/*implicitSDF.getSample(currentPos, s1);
					implicitSDF.getSample(currentPos + Ogre::Vector3(0, stepSize, 0), s2);
					vAssert(!signsAreEqual(s1.signedDistance, s2.signedDistance));
					float w = getInterpolationWeight(s1.signedDistance, s2.signedDistance);
					Ogre::Vector3 intersectionPos = MathMisc::linearInterpolation(currentPos, currentPos + Ogre::Vector3(0, stepSize, 0), w);*/
					m_SurfaceEdges.emplace_back(area.m_MinPos, iPos, 1, currentPos + Ogre::Vector3(0, stepSize * 0.5f, 0), implicitSDF, sharedVertices);
					// m_SurfaceEdges.emplace_back(index, indexOf(x, y + 1, z), currentPos);
					// m_SurfaceEdges.back().surfacePos.y = MathMisc::linearInterpolation(currentPos.y, currentPos.y + stepSize, getInterpolationWeight(s1.signedDistance, s2.signedDistance));
				}
				if (z < LEAF_SIZE_1D_INNER && m_Signs[index] != m_Signs[indexOf(x, y, z + 1)])
				{
					/*implicitSDF.getSample(currentPos, s1);
					implicitSDF.getSample(currentPos + Ogre::Vector3(0, 0, stepSize), s2);
					vAssert(!signsAreEqual(s1.signedDistance, s2.signedDistance));
					float w = getInterpolationWeight(s1.signedDistance, s2.signedDistance);
					Ogre::Vector3 intersectionPos = MathMisc::linearInterpolation(currentPos, currentPos + Ogre::Vector3(0, 0, stepSize), w);*/
					m_SurfaceEdges.emplace_back(area.m_MinPos, iPos, 2, currentPos + Ogre::Vector3(0, 0, stepSize * 0.5f), implicitSDF, sharedVertices);
					// m_SurfaceEdges.emplace_back(index, indexOf(x, y, z + 1), currentPos);
					// m_SurfaceEdges.back().surfacePos.z = MathMisc::linearInterpolation(currentPos.z, currentPos.z + stepSize, getInterpolationWeight(s1.signedDistance, s2.signedDistance));
				}
				currentPos.z += stepSize;
			}
			currentPos.z = area.m_MinRealPos.z;
			currentPos.y += stepSize;
		}
		currentPos.y = area.m_MinRealPos.y;
		currentPos.x += stepSize;
	}

	for (unsigned int x = 0; x < LEAF_SIZE_1D_INNER; x++)
	{
		for (unsigned int y = 0; y < LEAF_SIZE_1D_INNER; y++)
		{
			for (unsigned int z = 0; z < LEAF_SIZE_1D_INNER; z++)
			{
				index = indexOf(x, y, z);
				std::bitset<8> signs;
				signs[0] = m_Signs[index];
				signs[1] = m_Signs[index + 1];
				signs[2] = m_Signs[index + LEAF_SIZE_1D];
				signs[3] = m_Signs[index + LEAF_SIZE_1D + 1];
				signs[4] = m_Signs[index + LEAF_SIZE_2D];
				signs[5] = m_Signs[index + LEAF_SIZE_2D + 1];
				signs[6] = m_Signs[index + LEAF_SIZE_2D + LEAF_SIZE_1D];
				signs[7] = m_Signs[index + LEAF_SIZE_2D + LEAF_SIZE_1D + 1];
				if (signs.any() && !signs.all())
					m_SurfaceCubes.push_back(index);
			}
		}
	}
}

OctreeSF::GridNode::~GridNode()
{
}

void OctreeSF::GridNode::countMemory(int& memoryCounter) const
{
	memoryCounter += sizeof(*this) + (int)m_SurfaceEdges.capacity() * sizeof(SurfaceEdge) + (int)m_SurfaceCubes.capacity() * sizeof(unsigned short);
	for (auto i = m_SurfaceEdges.begin(); i != m_SurfaceEdges.end(); ++i)
	{
		if (i->ownsVertex())
			memoryCounter += sizeof(SharedSurfaceVertex);
	}
}

OctreeSF::Node* OctreeSF::createNode(const Area& area, const SignedDistanceField3D& implicitSDF, Vector3iHashGrid<SharedSurfaceVertex*>* sharedVertices)
{
	bool needsSubdivision = implicitSDF.cubeNeedsSubdivision(area);
	if (area.m_SizeExpo <= LEAF_EXPO && needsSubdivision)
		return new GridNode(area, implicitSDF, sharedVertices);

	if (needsSubdivision)
		return new InnerNode(area, implicitSDF, sharedVertices);

	return new EmptyNode(area, implicitSDF);
}

void OctreeSF::InnerNode::countNodes(int& counter) const
{
	counter++;
	for (int i = 0; i < 8; i++)
		m_Children[i]->countNodes(counter);
}

void OctreeSF::InnerNode::countLeaves(int& counter) const
{
	for (int i = 0; i < 8; i++)
		m_Children[i]->countLeaves(counter);
}

void OctreeSF::InnerNode::countMemory(int& counter) const
{
	counter += sizeof(*this);
	for (int i = 0; i < 8; i++)
		m_Children[i]->countMemory(counter);
}

void OctreeSF::GridNode::generateVertices(const Area& area, vector<Vertex>& vertices)
{
	// vertices.reserve(vertices.size() + m_SurfaceEdges.size());
	for (auto i = m_SurfaceEdges.begin(); i != m_SurfaceEdges.end(); ++i)
	{
		if (i->ownsVertex())
		{
			i->sharedVertex->vertexIndex = vertices.size();
			vertices.push_back(i->sharedVertex->vertex);
		}
	}
}

#include "TriangleLookupTable.h"
void OctreeSF::GridNode::generateIndices(const Area& area, vector<unsigned int>& indices) const
{
	static const SurfaceEdge* surfaceEdgeMaps[LEAF_SIZE_3D * 3];
	for (auto i = m_SurfaceEdges.begin(); i != m_SurfaceEdges.end(); ++i)
	{
		surfaceEdgeMaps[LEAF_SIZE_3D * i->direction + i->edgeIndex1] = &(*i);
	}

	for (auto i = m_SurfaceCubes.begin(); i != m_SurfaceCubes.end(); ++i)
	{
		int index = *i;
		unsigned char corners = 0;
		corners |= (unsigned char)m_Signs[index];
		corners |= ((unsigned char)m_Signs[index + 1] << 1);
		corners |= ((unsigned char)m_Signs[index + LEAF_SIZE_1D] << 2);
		corners |= ((unsigned char)m_Signs[index + LEAF_SIZE_1D + 1] << 3);
		corners |= ((unsigned char)m_Signs[index + LEAF_SIZE_2D] << 4);
		corners |= ((unsigned char)m_Signs[index + LEAF_SIZE_2D + 1] << 5);
		corners |= ((unsigned char)m_Signs[index + LEAF_SIZE_2D + LEAF_SIZE_1D] << 6);
		corners |= ((unsigned char)m_Signs[index + LEAF_SIZE_2D + LEAF_SIZE_1D + 1] << 7);
		const std::vector<Triangle<int> >& tris = TLT::getSingleton().indexTable[corners];
		for (auto i2 = tris.begin(); i2 != tris.end(); ++i2)
		{
			const TLT::DirectedEdge& p1 = TLT::getSingleton().directedEdges[i2->p1];
			const TLT::DirectedEdge& p2 = TLT::getSingleton().directedEdges[i2->p2];
			const TLT::DirectedEdge& p3 = TLT::getSingleton().directedEdges[i2->p3];
			indices.push_back((int)(surfaceEdgeMaps[LEAF_SIZE_3D * p1.direction
				+ index
				+ (p1.minCornerIndex & 1)
				+ ((p1.minCornerIndex & 2) >> 1) * LEAF_SIZE_1D
				+ ((p1.minCornerIndex & 4) >> 2) * LEAF_SIZE_2D]->sharedVertex->vertexIndex));

			indices.push_back((int)(surfaceEdgeMaps[LEAF_SIZE_3D * p2.direction
				+ index
				+ (p2.minCornerIndex & 1)
				+ ((p2.minCornerIndex & 2) >> 1) * LEAF_SIZE_1D
				+ ((p2.minCornerIndex & 4) >> 2) * LEAF_SIZE_2D]->sharedVertex->vertexIndex));

			indices.push_back((int)(surfaceEdgeMaps[LEAF_SIZE_3D * p3.direction
				+ index
				+ (p3.minCornerIndex & 1)
				+ ((p3.minCornerIndex & 2) >> 1) * LEAF_SIZE_1D
				+ ((p3.minCornerIndex & 4) >> 2) * LEAF_SIZE_2D]->sharedVertex->vertexIndex));
		}
	}
}

/*void OctreeSF::GridNode::getCubesToMarch(const Area& area, vector<Cube>& cubes) const
{
	static Cube localCubes[LEAF_SIZE_3D];
	std::bitset<LEAF_SIZE_3D> addedCubes;
	for (auto i = m_SurfaceEdges.begin(); i != m_SurfaceEdges.end(); ++i)
	{
		vAssert(i->index1 < LEAF_SIZE_3D);
		Vector3i pos1 = fromIndex(i->index1);
		Vector3i pos2 = pos1;
		pos2[i->direction] += 1;
		Vector3i pos = pos1;
		int index = indexOf(pos);
		if (pos.x >= 0 && pos.y >= 0 && pos.z >= 0
			&& pos.x < LEAF_SIZE_1D_INNER && pos.y < LEAF_SIZE_1D_INNER && pos.z < LEAF_SIZE_1D_INNER)
		{
			Cube& cube = localCubes[index];
			if (!addedCubes[index])
			{
				for (int j = 0; j < 8; j++)
				{
					cube.cornerSamples[j] = m_Signs[indexOf(pos + Vector3i::fromBitMask(j))] ? &Sample::insideSingleton() : &Sample::outsideSingleton();
				}
			}
			vAssert(!allSignsAreEqual(cube.cornerSamples));
			addedCubes[index] = true;
			cube.posMin = area.m_MinPos + pos;
			cube.cornerSamples[(pos1 - pos).toBitMask()] = &i->s1;
			cube.cornerSamples[(pos2 - pos).toBitMask()] = &i->s2;
		}

		pos = pos1;
		pos[(i->direction + 1) % 3] -= 1;
		index = indexOf(pos);
		if (pos.x >= 0 && pos.y >= 0 && pos.z >= 0
			&& pos.x < LEAF_SIZE_1D_INNER && pos.y < LEAF_SIZE_1D_INNER && pos.z < LEAF_SIZE_1D_INNER)
		{
			Cube& cube = localCubes[index];
			if (!addedCubes[index])
			{
				for (int j = 0; j < 8; j++)
				{
					cube.cornerSamples[j] = m_Signs[indexOf(pos + Vector3i::fromBitMask(j))] ? &Sample::insideSingleton() : &Sample::outsideSingleton();
				}
			}
			vAssert(!allSignsAreEqual(cube.cornerSamples));
			addedCubes[index] = true;
			cube.posMin = area.m_MinPos + pos;
			cube.cornerSamples[(pos1 - pos).toBitMask()] = &i->s1;
			cube.cornerSamples[(pos2 - pos).toBitMask()] = &i->s2;
		}

		pos = pos1;
		pos[(i->direction + 2) % 3] -= 1;
		index = indexOf(pos);
		if (pos.x >= 0 && pos.y >= 0 && pos.z >= 0
			&& pos.x < LEAF_SIZE_1D_INNER && pos.y < LEAF_SIZE_1D_INNER && pos.z < LEAF_SIZE_1D_INNER)
		{
			Cube& cube = localCubes[index];
			if (!addedCubes[index])
			{
				for (int j = 0; j < 8; j++)
				{
					cube.cornerSamples[j] = m_Signs[indexOf(pos + Vector3i::fromBitMask(j))] ? &Sample::insideSingleton() : &Sample::outsideSingleton();
				}
			}
			vAssert(!allSignsAreEqual(cube.cornerSamples));
			addedCubes[index] = true;
			cube.posMin = area.m_MinPos + pos;
			cube.cornerSamples[(pos1 - pos).toBitMask()] = &i->s1;
			cube.cornerSamples[(pos2 - pos).toBitMask()] = &i->s2;
		}

		pos = pos1;
		pos[(i->direction + 1) % 3] -= 1;
		pos[(i->direction + 2) % 3] -= 1;
		index = indexOf(pos);
		if (pos.x >= 0 && pos.y >= 0 && pos.z >= 0
			&& pos.x < LEAF_SIZE_1D_INNER && pos.y < LEAF_SIZE_1D_INNER && pos.z < LEAF_SIZE_1D_INNER)
		{
			Cube& cube = localCubes[index];
			if (!addedCubes[index])
			{
				for (int j = 0; j < 8; j++)
				{
					cube.cornerSamples[j] = m_Signs[indexOf(pos + Vector3i::fromBitMask(j))] ? &Sample::insideSingleton() : &Sample::outsideSingleton();
				}
			}
			vAssert(!allSignsAreEqual(cube.cornerSamples));
			addedCubes[index] = true;
			cube.posMin = area.m_MinPos + pos;
			cube.cornerSamples[(pos1 - pos).toBitMask()] = &i->s1;
			cube.cornerSamples[(pos2 - pos).toBitMask()] = &i->s2;
		}
	}
	for (int i = 0; i < LEAF_SIZE_3D; i++)
	{
		if (addedCubes[i])
		{
			cubes.push_back(localCubes[i]);
		}
	}
}*/

void OctreeSF::InnerNode::generateVertices(const Area& area, vector<Vertex>& vertices)
{
	Area subAreas[8];
	area.getSubAreas(subAreas);
	for (int i = 0; i < 8; i++)
		m_Children[i]->generateVertices(subAreas[i], vertices);
}
void OctreeSF::InnerNode::generateIndices(const Area& area, vector<unsigned int>& indices) const
{
	Area subAreas[8];
	area.getSubAreas(subAreas);
	for (int i = 0; i < 8; i++)
		m_Children[i]->generateIndices(subAreas[i], indices);
}

void OctreeSF::InnerNode::invert()
{
	for (int i = 0; i < 8; i++)
		m_Children[i]->invert();
}

void OctreeSF::EmptyNode::invert()
{
	m_Sign = !m_Sign;
}

void OctreeSF::GridNode::invert()
{
	m_Signs.flip();
}

void OctreeSF::GridNode::addEdgesAndVerticesWithSignChange(const std::vector<SurfaceEdge>& edgesIn, const std::vector<unsigned short>& cubesIn)
{
	for (auto i = edgesIn.begin(); i != edgesIn.end(); i++)
	{
		if (m_Signs[i->edgeIndex1] != m_Signs[i->edgeIndex2])
			m_SurfaceEdges.push_back(*i);
	}

	for (auto i = cubesIn.begin(); i != cubesIn.end(); i++)
	{
		std::bitset<8> signs;
		signs[0] = m_Signs[*i];
		signs[1] = m_Signs[*i + 1];
		signs[2] = m_Signs[*i + LEAF_SIZE_1D];
		signs[3] = m_Signs[*i + LEAF_SIZE_1D + 1];
		signs[4] = m_Signs[*i + LEAF_SIZE_2D];
		signs[5] = m_Signs[*i + LEAF_SIZE_2D + 1];
		signs[6] = m_Signs[*i + LEAF_SIZE_2D + LEAF_SIZE_1D];
		signs[7] = m_Signs[*i + LEAF_SIZE_2D + LEAF_SIZE_1D + 1];
		if (signs.any() && !signs.all())
			m_SurfaceCubes.push_back(*i);
	}
}

OctreeSF::Node* OctreeSF::intersect(Node* node, const SignedDistanceField3D& implicitSDF, const Area& area)
{
	/*bool needsSubdivision = implicitSDF.cubeNeedsSubdivision(area);
	if (node->getNodeType() == Node::INNER && needsSubdivision)
	{
		InnerNode* innerNode = (InnerNode*)node;
		Area subAreas[8];
		area.getSubAreas(subAreas);
		for (int i = 0; i < 8; i++)
			innerNode->m_Children[i] = intersect(innerNode->m_Children[i], implicitSDF, subAreas[i]);
		return node;
	}
	if (!needsSubdivision)
	{
		if (implicitSDF.getSign(area.getCornerVecs(0).second))
			return node;
		delete node;
		return createNode(area, implicitSDF);
	}
	if (node->getNodeType() == Node::EMPTY)
	{
		EmptyNode* emptyNode = (EmptyNode*)node;
		if (!emptyNode->m_Sign)
			return node;
		delete node;
		return createNode(area, implicitSDF);

	}

	GridNode* gridNode = (GridNode*)node;
	GridNode otherGridNode(area, implicitSDF);
	auto edgeCopy = gridNode->m_SurfaceEdges;
	auto cubesCopy = gridNode->m_SurfaceCubes;
	gridNode->m_SurfaceEdges.clear();
	gridNode->m_SurfaceCubes.clear();
	gridNode->m_Signs &= otherGridNode.m_Signs;
	gridNode->addEdgesAndVerticesWithSignChange(edgeCopy, cubesCopy);
	gridNode->addEdgesAndVerticesWithSignChange(otherGridNode.m_SurfaceEdges, otherGridNode.m_SurfaceCubes);*/
	return node;
}

OctreeSF::Node* OctreeSF::subtract(Node* node, const SignedDistanceField3D& implicitSDF, const Area& area)
{
	/*bool needsSubdivision = implicitSDF.cubeNeedsSubdivision(area);
	if (node->getNodeType() == Node::INNER && needsSubdivision)
	{
		InnerNode* innerNode = (InnerNode*)node;
		Area subAreas[8];
		area.getSubAreas(subAreas);
		for (int i = 0; i < 8; i++)
			innerNode->m_Children[i] = subtract(innerNode->m_Children[i], implicitSDF, subAreas[i]);
		return node;
	}
	if (!needsSubdivision)
	{
		if (!implicitSDF.getSign(area.getCornerVecs(0).second))
			return node;
		delete node;
		Node* newNode = createNode(area, implicitSDF);
		newNode->invert();
		return newNode;
	}
	if (node->getNodeType() == Node::EMPTY)
	{
		EmptyNode* emptyNode = (EmptyNode*)node;
		if (!emptyNode->m_Sign)
			return node;
		delete node;
		Node* newNode = createNode(area, implicitSDF);
		newNode->invert();
		return newNode;

	}

	GridNode* gridNode = (GridNode*)node;
	GridNode otherGridNode(area, implicitSDF);
	otherGridNode.invert();
	auto edgeCopy = gridNode->m_SurfaceEdges;
	auto cubesCopy = gridNode->m_SurfaceCubes;
	gridNode->m_SurfaceEdges.clear();
	gridNode->m_SurfaceCubes.clear();
	gridNode->m_Signs &= otherGridNode.m_Signs;
	gridNode->addEdgesAndVerticesWithSignChange(edgeCopy, cubesCopy);
	gridNode->addEdgesAndVerticesWithSignChange(otherGridNode.m_SurfaceEdges, otherGridNode.m_SurfaceCubes);*/
	return node;
}

OctreeSF::Node* OctreeSF::intersectAlignedNode(Node* node, Node* otherNode, const Area& area)
{
	if (node->getNodeType() == Node::INNER && otherNode->getNodeType() == Node::INNER)
	{
		InnerNode* innerNode = (InnerNode*)node;
		InnerNode* otherInnerNode = (InnerNode*)otherNode;
		Area subAreas[8];
		area.getSubAreas(subAreas);
		for (int i = 0; i < 8; i++)
			innerNode->m_Children[i] = intersectAlignedNode(innerNode->m_Children[i], otherInnerNode->m_Children[i], subAreas[i]);
		return node;
	}
	if (otherNode->getNodeType() == Node::EMPTY)
	{
		EmptyNode* otherEmptyNode = (EmptyNode*)otherNode;
		if (otherEmptyNode->m_Sign)
			return node;
		delete node;
		return otherNode->clone();
	}
	if (node->getNodeType() == Node::EMPTY)
	{
		EmptyNode* emptyNode = (EmptyNode*)node;
		if (!emptyNode->m_Sign)
			return node;
		delete node;
		return otherNode->clone();

	}

	GridNode* gridNode = (GridNode*)node;
	GridNode* otherGridNode = (GridNode*)otherNode;
	auto edgeCopy = gridNode->m_SurfaceEdges;
	auto cubesCopy = gridNode->m_SurfaceCubes;
	gridNode->m_SurfaceEdges.clear();
	gridNode->m_SurfaceCubes.clear();
	gridNode->m_Signs &= otherGridNode->m_Signs;
	gridNode->addEdgesAndVerticesWithSignChange(edgeCopy, cubesCopy);
	gridNode->addEdgesAndVerticesWithSignChange(otherGridNode->m_SurfaceEdges, otherGridNode->m_SurfaceCubes);
	return node;
}

OctreeSF::Node* OctreeSF::subtractAlignedNode(Node* node, Node* otherNode, const Area& area)
{
	if (node->getNodeType() == Node::INNER && otherNode->getNodeType() == Node::INNER)
	{
		InnerNode* innerNode = (InnerNode*)node;
		InnerNode* otherInnerNode = (InnerNode*)otherNode;
		Area subAreas[8];
		area.getSubAreas(subAreas);
		for (int i = 0; i < 8; i++)
			innerNode->m_Children[i] = subtractAlignedNode(innerNode->m_Children[i], otherInnerNode->m_Children[i], subAreas[i]);
		return node;
	}
	if (otherNode->getNodeType() == Node::EMPTY)
	{
		EmptyNode* otherEmptyNode = (EmptyNode*)otherNode;
		if (!otherEmptyNode->m_Sign)
			return node;
		delete node;
		Node* inverted = otherNode->clone();
		inverted->invert();
		return inverted;
	}
	if (node->getNodeType() == Node::EMPTY)
	{
		EmptyNode* emptyNode = (EmptyNode*)node;
		if (!emptyNode->m_Sign)
			return node;
		delete node;
		Node* inverted = otherNode->clone();
		inverted->invert();
		return inverted;

	}

	GridNode* gridNode = (GridNode*)node;
	GridNode* otherGridNode = (GridNode*)otherNode;
	otherGridNode->invert();
	auto edgeCopy = gridNode->m_SurfaceEdges;
	auto cubesCopy = gridNode->m_SurfaceCubes;
	gridNode->m_SurfaceEdges.clear();
	gridNode->m_SurfaceCubes.clear();
	gridNode->m_Signs &= otherGridNode->m_Signs;
	gridNode->addEdgesAndVerticesWithSignChange(edgeCopy, cubesCopy);
	gridNode->addEdgesAndVerticesWithSignChange(otherGridNode->m_SurfaceEdges, otherGridNode->m_SurfaceCubes);
	return node;
}

OctreeSF::Node* OctreeSF::mergeAlignedNode(Node* node, Node* otherNode, const Area& area)
{
	if (node->getNodeType() == Node::INNER && otherNode->getNodeType() == Node::INNER)
	{
		InnerNode* innerNode = (InnerNode*)node;
		InnerNode* otherInnerNode = (InnerNode*)otherNode;
		Area subAreas[8];
		area.getSubAreas(subAreas);
		for (int i = 0; i < 8; i++)
			innerNode->m_Children[i] = mergeAlignedNode(innerNode->m_Children[i], otherInnerNode->m_Children[i], subAreas[i]);
		return node;
	}
	if (otherNode->getNodeType() == Node::EMPTY)
	{
		EmptyNode* otherEmptyNode = (EmptyNode*)otherNode;
		if (!otherEmptyNode->m_Sign)
			return node;
		delete node;
		return otherNode->clone();
	}
	if (node->getNodeType() == Node::EMPTY)
	{
		EmptyNode* emptyNode = (EmptyNode*)node;
		if (emptyNode->m_Sign)
			return node;
		delete node;
		return otherNode->clone();

	}

	GridNode* gridNode = (GridNode*)node;
	GridNode* otherGridNode = (GridNode*)otherNode;
	auto edgeCopy = gridNode->m_SurfaceEdges;
	auto cubesCopy = gridNode->m_SurfaceCubes;
	gridNode->m_SurfaceEdges.clear();
	gridNode->m_SurfaceCubes.clear();
	gridNode->m_Signs |= otherGridNode->m_Signs;
	gridNode->addEdgesAndVerticesWithSignChange(edgeCopy, cubesCopy);
	gridNode->addEdgesAndVerticesWithSignChange(otherGridNode->m_SurfaceEdges, otherGridNode->m_SurfaceCubes);
	return node;
}

std::shared_ptr<OctreeSF> OctreeSF::sampleSDF(SignedDistanceField3D* otherSDF, int maxDepth)
{
	AABB aabb = otherSDF->getAABB();
	Ogre::Vector3 epsilonVec(0.000001f, 0.000001f, 0.000001f);
	aabb.min -= epsilonVec;
	aabb.max += epsilonVec;
	return sampleSDF(otherSDF, aabb, maxDepth);
}

std::shared_ptr<OctreeSF> OctreeSF::sampleSDF(SignedDistanceField3D* otherSDF, const AABB& aabb, int maxDepth)
{
	std::shared_ptr<OctreeSF> octreeSF = std::make_shared<OctreeSF>();
	Ogre::Vector3 aabbSize = aabb.getMax() - aabb.getMin();
	float cubeSize = std::max(std::max(aabbSize.x, aabbSize.y), aabbSize.z);
	octreeSF->m_CellSize = cubeSize / (1 << maxDepth);
	otherSDF->prepareSampling(aabb, octreeSF->m_CellSize);
	octreeSF->m_RootArea = Area(Vector3i(0, 0, 0), maxDepth, aabb.getMin(), cubeSize);
	Vector3iHashGrid<SharedSurfaceVertex*> sharedVertices[3];
	octreeSF->m_RootNode = octreeSF->createNode(octreeSF->m_RootArea, *otherSDF, sharedVertices);
	return octreeSF;
}

float OctreeSF::getInverseCellSize()
{
	return (float)(1 << m_RootArea.m_SizeExpo) / m_RootArea.m_RealSize;
}

AABB OctreeSF::getAABB() const
{
	return m_RootArea.toAABB();
}

#include "Profiler.h"
std::shared_ptr<Mesh> OctreeSF::generateMesh()
{
	std::shared_ptr<Mesh> mesh = std::make_shared<Mesh>();
	int numLeaves = countLeaves();
	mesh->vertexBuffer.reserve(numLeaves * LEAF_SIZE_2D_INNER * 2);	// reasonable upper bound
	mesh->indexBuffer.reserve(numLeaves * LEAF_SIZE_2D_INNER * 3);
	auto ts = Profiler::timestamp();
	m_RootNode->generateVertices(m_RootArea, mesh->vertexBuffer);
	std::cout << "Num vertices: " << mesh->vertexBuffer.size() << std::endl;
	m_RootNode->generateIndices(m_RootArea, mesh->indexBuffer);
	Profiler::printJobDuration("generateMesh", ts);
	return mesh;
}

bool OctreeSF::intersectsSurface(const AABB& aabb) const
{
	if (m_TriangleCache.getBVH())
		return m_TriangleCache.getBVH()->intersectsAABB(aabb);
	return true;
}

void OctreeSF::subtract(SignedDistanceField3D* otherSDF)
{
	otherSDF->prepareSampling(m_RootArea.toAABB(), m_CellSize);
	auto ts = Profiler::timestamp();
	m_RootNode = subtract(m_RootNode, *otherSDF, m_RootArea);
	Profiler::printJobDuration("Subtraction", ts);
}

void OctreeSF::intersect(SignedDistanceField3D* otherSDF)
{
	otherSDF->prepareSampling(m_RootArea.toAABB(), m_CellSize);
	auto ts = Profiler::timestamp();
	m_RootNode = intersect(m_RootNode, *otherSDF, m_RootArea);
	Profiler::printJobDuration("Intersection", ts);
}

void OctreeSF::intersectAlignedOctree(OctreeSF* otherOctree)
{
	m_RootNode = intersectAlignedNode(m_RootNode, otherOctree->m_RootNode, m_RootArea);
}

void OctreeSF::subtractAlignedOctree(OctreeSF* otherOctree)
{
	m_RootNode = subtractAlignedNode(m_RootNode, otherOctree->m_RootNode, m_RootArea);
}

void OctreeSF::mergeAlignedOctree(OctreeSF* otherOctree)
{
	m_RootNode = mergeAlignedNode(m_RootNode, otherOctree->m_RootNode, m_RootArea);
}

void OctreeSF::resize(const AABB& aabb)
{
	/*	while (!m_RootArea.toAABB().containsPoint(aabb.min))
	{	// need to resize octree
	m_RootArea.m_MinPos = m_RootArea.m_MinPos - Vector3i(1 << m_RootArea.m_SizeExpo, 1 << m_RootArea.m_SizeExpo, 1 << m_RootArea.m_SizeExpo);
	m_RootArea.m_MinRealPos -= Ogre::Vector3(m_RootArea.m_RealSize, m_RootArea.m_RealSize, m_RootArea.m_RealSize);
	m_RootArea.m_RealSize *= 2.0f;
	m_RootArea.m_SizeExpo++;
	Node* oldRoot = m_RootNode;
	m_RootNode = allocNode();
	m_RootNode->m_Children[7] = oldRoot;

	// need to fill in some signed distance values for the new area
	for (int i = 0; i < 7; i++)
	{
	Vector3i offset = Vector3i((i & 4) != 0, (i & 2) != 0, (i & 1) != 0) * (1 << m_RootArea.m_SizeExpo);
	m_SDFValues[m_RootArea.m_MinPos + offset] = -m_RootArea.m_RealSize;
	}
	interpolateLeaf(m_RootArea);
	}
	while (!m_RootArea.toAABB().containsPoint(aabb.max))
	{	// need to resize octree
	m_RootArea.m_RealSize *= 2.0f;
	m_RootArea.m_SizeExpo++;
	Node* oldRoot = m_RootNode;
	m_RootNode = allocNode();
	m_RootNode->m_Children[0] = oldRoot;

	// need to fill in some signed distance values for the new area
	for (int i = 1; i < 8; i++)
	{
	Vector3i offset = Vector3i((i & 4) != 0, (i & 2) != 0, (i & 1) != 0) * (1 << m_RootArea.m_SizeExpo);
	m_SDFValues[m_RootArea.m_MinPos + offset] = -m_RootArea.m_RealSize;
	}
	interpolateLeaf(m_RootArea);
	}*/
}

void OctreeSF::merge(SignedDistanceField3D* otherSDF)
{
	// this is not an optimal resize policy but it should work
	// it is recommended to avoid resizes anyway
	/*	resize(otherSDF->getAABB());

	otherSDF->prepareSampling(m_RootArea.toAABB(), m_CellSize);
	SignedDistanceGrid newSDF;
	m_RootNode = merge(m_RootNode, m_RootArea, *otherSDF, newSDF, SignedDistanceGrid());
	for (auto i = newSDF.begin(); i != newSDF.end(); i++)
	{
	for (auto i2 = i->begin(); i2 != i->end(); i2++)
	{
	m_SDFValues[i2->first] = i2->second;
	}
	}*/
}

OctreeSF::OctreeSF(const OctreeSF& other)
{
	m_RootNode = other.m_RootNode->clone();
	m_RootArea = other.m_RootArea;
	m_CellSize = other.m_CellSize;
	m_TriangleCache = other.m_TriangleCache;
}

OctreeSF::~OctreeSF()
{
	if (m_RootNode)
		delete m_RootNode;
}

std::shared_ptr<OctreeSF> OctreeSF::clone()
{
	return std::make_shared<OctreeSF>(*this);
}

int OctreeSF::countNodes()
{
	int counter = 0;
	m_RootNode->countNodes(counter);
	return counter;
}

int OctreeSF::countLeaves()
{
	int counter = 0;
	m_RootNode->countLeaves(counter);
	return counter;
}

int OctreeSF::countMemory()
{
	int counter = 0;
	m_RootNode->countMemory(counter);
	return counter;
}

Ogre::Vector3 OctreeSF::getCenterOfMass(float& totalMass)
{
	Ogre::Vector3 centerOfMass(0, 0, 0);
	totalMass = 0;
	m_RootNode->sumPositionsAndMass(m_RootArea, centerOfMass, totalMass);
	if (totalMass > 0) centerOfMass /= totalMass;
	return centerOfMass;
}

Ogre::Vector3 OctreeSF::getCenterOfMass()
{
	float mass = 0.0f;
	return getCenterOfMass(mass);
}

void OctreeSF::simplify()
{
	// int nodeMask;
	// m_RootNode = simplifyNode(m_RootNode, m_RootArea, nodeMask);
}

void OctreeSF::generateTriangleCache()
{
	auto mesh = generateMesh();
	auto transformedMesh = std::make_shared<TransformedMesh>(mesh);
	std::cout << "Computing cache" << std::endl;
	mesh->computeTriangleNormals();
	transformedMesh->computeCache();
	m_TriangleCache.clearMeshes();
	m_TriangleCache.addMesh(transformedMesh);
	std::cout << "Generating BVH" << std::endl;
	m_TriangleCache.generateBVH<AABB>();
	std::cout << "Finished generating BVH" << std::endl;
}