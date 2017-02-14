/**
 * @file
 */

#pragma once

#include "polyvox/Region.h"
#include "polyvox/Mesh.h"

namespace voxel {

class Octree;
class SurfaceExtractionTask;

typedef uint16_t NodeIndex;

class OctreeNode {
	friend class Octree;
private:
	NodeIndex _parent;
	/** own index in the nodes array */
	NodeIndex _self = 0u;
	NodeIndex _children[2][2][2];

	uint8_t _height = 0u; // Zero for leaf nodes.
	bool _renderThisNode = false;
	bool _active = false;

	// Use flags here?
	bool _canRenderNodeOrChildren = false;
	bool _isLeaf = false;

	std::shared_ptr<Mesh> _mesh;
	std::shared_ptr<Mesh> _waterMesh;

public:
	OctreeNode(const Region& region, NodeIndex parent, Octree* octree);

	OctreeNode* getActiveChildNode(uint32_t childX, uint32_t childY, uint32_t childZ) const;
	OctreeNode* getChildNode(uint8_t x, uint8_t y, uint8_t z) const;
	OctreeNode* getParentNode() const;

	const Mesh* getWaterMesh();
	const Mesh* getMesh();
	void setMesh(const std::shared_ptr<Mesh>& mesh, const std::shared_ptr<Mesh>& waterMesh);

	template<class FUNC>
	void visitChildren(FUNC&& func) {
		for (uint8_t z = 0; z < 2; ++z) {
			for (uint8_t y = 0; y < 2; ++y) {
				for (uint8_t x = 0; x < 2; ++x) {
					OctreeNode* child = getChildNode(x, y, z);
					func(x, y, z, child);
				}
			}
		}
	}

	template<class FUNC>
	void visitExistingChildren(FUNC&& func) {
		for (uint8_t z = 0; z < 2; ++z) {
			for (uint8_t y = 0; y < 2; ++y) {
				for (uint8_t x = 0; x < 2; ++x) {
					OctreeNode* child = getChildNode(x, y, z);
					if (child == nullptr) {
						continue;
					}
					func(x, y, z, child);
				}
			}
		}
	}

	uint8_t height() const;

	bool isActive() const;
	void setActive(bool active);

	bool renderThisNode() const;
	void setRenderThisNode(bool render);

	bool isMeshUpToDate() const;
	/**
	 * @brief We are scheduled for an update if being scheduled was the most recent thing that happened.
	 */
	bool isScheduledForUpdate() const;

	void updateFromCompletedTask(SurfaceExtractionTask* completedTask);

	Region _region;
	uint32_t _lastScheduledForUpdate = 0u;

	// The values of these few initialisations is important
	// to make sure the node is set to an 'out of date'
	// state which will then try to update.
	uint32_t _structureLastChanged = 1u;
	uint32_t _propertiesLastChanged = 1u;
	uint32_t _meshLastChanged = 1u;
	uint32_t _dataLastModified = 2u;
	uint32_t _nodeOrChildrenLastChanged = 1u;

	Octree* _octree;
	SurfaceExtractionTask* _lastSurfaceExtractionTask = nullptr;
};

inline bool OctreeNode::renderThisNode() const {
	return _renderThisNode;
}

inline bool OctreeNode::isActive() const {
	return _active;
}

inline uint8_t OctreeNode::height() const {
	return _height;
}

}
