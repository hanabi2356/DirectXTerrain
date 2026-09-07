#pragma once
#include "TextRenderer.h"
#include "Scene.h"     // SampleId
#include <string>
#include <vector>

class MenuScene
{
public:
	MenuScene();
	void OnKeyDown(WPARAM key);
	void OnMouseMove(int x, int y);
	void OnMouseDown(int x, int y);
	void Render(TextRenderer& text, UINT screenWidth, UINT screenHeight);
	// 선택된 샘플을 1회만 반환한다. 선택이 없으면 SampleId::None.
	SampleId ConsumeSelection();
	// TextRenderer::Initialize 에 넘길 문자 집합을 메뉴 문자열에서 만든다.
	std::wstring BuildCharset() const;
	static const wchar_t* GetSampleName(SampleId id);
private:
	struct Rect
	{
		float left = 0.0f;
		float top = 0.0f;
		float right = 0.0f;
		float bottom = 0.0f;
		bool Contains(float x, float y) const
		{
			return x >= left && x <= right && y >= top && y <= bottom;
		}
	};
	struct MenuItem
	{
		SampleId id;
		std::wstring label;
		Rect rect;
	};
	int IndexOf(SampleId id) const;
	static constexpr float TitleY = 40.0f;
	static constexpr float GuideY = 78.0f;
	static constexpr float ItemsTop = 118.0f;
	static constexpr float ItemSpacing = 34.0f;
	std::wstring m_title = L"=== Terrain Showcase System ===";
	std::wstring m_guide = L"마우스 클릭 또는 키(1~9, 0, -)로 실행하는 지형 샘플을 선택하세요";
	std::vector<MenuItem> m_items;
	SampleId m_pending = SampleId::None;
	int m_hovered = -1;
	int m_focused = 0;
};
