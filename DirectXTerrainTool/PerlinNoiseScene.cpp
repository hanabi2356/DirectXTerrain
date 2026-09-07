#include "PerlinNoiseScene.h"
#include "Camera.h"

bool PerlinNoiseScene::Initialize(GraphicsCore& core)
{
	if (!m_pipeline.Initialize(core))
	{
		return false;
	}

	m_pipeline.SetHeightScale(HeightScale);

	const HeightField field = HeightField::MakePerlin(
		GridResolution, GridResolution, NoiseFrequency, NoiseAmplitude, NoiseOctaves);

	return m_mesh.Build(core, field, CellSize);
}

void PerlinNoiseScene::Render(GraphicsCore& core, const Camera& camera, bool wireframe)
{
	ID3D12GraphicsCommandList* commandList = core.GetCommandList();

	m_pipeline.Bind(commandList, core.GetFrameIndex(), camera, wireframe);
	m_mesh.Draw(commandList);
}
