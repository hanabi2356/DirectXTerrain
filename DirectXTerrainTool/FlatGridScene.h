#pragma once
#include "Scene.h"
#include "Terrain.h"
#include "TerrainPipeline.h"

// 1번 샘플: 높이가 모두 0 인 기본 평면 그리드.
// 지형 파이프라인이 제대로 도는지 확인하는 가장 단순한 형태이고,
// 이후 샘플들은 HeightField 만 바꿔 끼우면 되므로 이 클래스가 기준형이 된다.
class FlatGridScene : public Scene
{
public:
	bool Initialize(GraphicsCore& core) override;
	void Render(GraphicsCore& core, const Camera& camera, bool wireframe) override;

private:
	// 가로세로 정점 개수. 129 이면 칸은 128 x 128 이 된다.
	static constexpr UINT GridResolution = 129;

	// 칸 하나의 월드 크기. 전체 지형은 128 x 1.5 = 192 유닛이 된다.
	static constexpr float CellSize = 1.5f;

	TerrainPipeline m_pipeline;
	TerrainMesh m_mesh;
};
