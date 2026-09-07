#include "FlatGridScene.h"
#include "Camera.h"

bool FlatGridScene::Initialize(GraphicsCore& core)
{
	if (!m_pipeline.Initialize(core))
	{
		return false;
	}

	return m_mesh.Build(core, HeightField::MakeFlat(GridResolution, GridResolution), CellSize);
}

void FlatGridScene::Render(GraphicsCore& core, const Camera& camera, bool wireframe)
{
	ID3D12GraphicsCommandList* commandList = core.GetCommandList();

	m_pipeline.Bind(commandList, core.GetFrameIndex(), camera, wireframe);
	m_mesh.Draw(commandList);
}
