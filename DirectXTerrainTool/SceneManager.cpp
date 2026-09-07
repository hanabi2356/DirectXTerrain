#include "SceneManager.h"
#include "GraphicsCore.h"
#include "FlatGridScene.h"

std::unique_ptr<Scene> SceneManager::Create(SampleId id)
{
	switch (id)
	{
	case SampleId::FlatGrid:
		return std::make_unique<FlatGridScene>();

	// 남은 샘플은 여기에 한 줄씩 추가한다.
	default:
		return nullptr;
	}
}

bool SceneManager::Switch(GraphicsCore& core, SampleId id)
{
	// 이전 씬의 GPU 리소스를 놓기 전에 사용이 끝났는지 확인해야 한다.
	Clear(core);

	m_current = id;
	if (id == SampleId::None)
	{
		return true;
	}

	std::unique_ptr<Scene> scene = Create(id);
	if (!scene || !scene->Initialize(core))
	{
		return false;
	}

	m_scene = std::move(scene);
	return true;
}

void SceneManager::Clear(GraphicsCore& core)
{
	if (m_scene)
	{
		core.WaitForGpu();
		m_scene.reset();
	}
	m_current = SampleId::None;
}

void SceneManager::Update(float deltaTime)
{
	if (m_scene)
	{
		m_scene->Update(deltaTime);
	}
}

void SceneManager::Render(GraphicsCore& core, const Camera& camera, bool wireframe)
{
	if (m_scene)
	{
		m_scene->Render(core, camera, wireframe);
	}
}
