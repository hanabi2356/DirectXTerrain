#pragma once
#include "Scene.h"
#include "Terrain.h"
#include "TerrainPipeline.h"

// 1번 샘플: 높이가 모두 0 인 기본 평면 그리드.
// 이후 샘플들은 HeightField 만 바꿔 끼우면 되므로 이 클래스가 기준형이 된다.
class FlatGridScene : public Scene
{
public:
	bool Initialize(GraphicsCore& core) override;
	void Render(GraphicsCore& core, const Camera& camera, bool wireframe) override;

private:
	static constexpr UINT GridResolution = 129;
	static constexpr float CellSize = 1.5f;

	TerrainPipeline m_pipeline;
	TerrainMesh m_mesh;
};
