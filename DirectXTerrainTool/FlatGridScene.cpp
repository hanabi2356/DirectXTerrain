#include "FlatGridScene.h"
#include "Camera.h"

bool FlatGridScene::Initialize(GraphicsCore& core)
{
	// 셰이더 컴파일과 PSO 생성은 무거우므로 진입할 때 한 번만 한다.
	if (!m_pipeline.Initialize(core))
	{
		return false;
	}

	// 높이가 전부 0 인 높이 필드로 격자 메시를 만든다.
	return m_mesh.Build(core, HeightField::MakeFlat(GridResolution, GridResolution), CellSize);
}

void FlatGridScene::Render(GraphicsCore& core, const Camera& camera, bool wireframe)
{
	ID3D12GraphicsCommandList* commandList = core.GetCommandList();

	// 카메라 행렬을 상수 버퍼에 넣고 파이프라인을 바인딩한 뒤 메시를 그린다.
	m_pipeline.Bind(commandList, core.GetFrameIndex(), camera, wireframe);
	m_mesh.Draw(commandList);
}
