#include "MenuScene.h"
#include <algorithm>
#include <set>
using namespace DirectX;
namespace
{
	const XMFLOAT4 ColorNormal = { 0.92f, 0.94f, 1.00f, 1.0f };
	const XMFLOAT4 ColorAccent = { 1.00f, 0.85f, 0.35f, 1.0f };
	const XMFLOAT4 ColorGuide = { 0.80f, 0.86f, 1.00f, 1.0f };
}
MenuScene::MenuScene()
{
	// 항목 순서가 곧 화면 순서이고, 숫자 키 배정도 이 순서를 따른다.
	m_items = {
		{ SampleId::FlatGrid,         L"1. 기본 평면 그리드 (Basic Flat Grid)", {} },
		{ SampleId::PerlinNoise,      L"2. 펄린 노이즈 지형 (Perlin Noise)", {} },
		{ SampleId::HeightMap,        L"3. 높이맵 이미지 지형 (HeightMap)", {} },
		{ SampleId::TextureSplatting, L"4. 텍스처 스플래팅 정점 경사도/높이 기반 (Texture Splatting)", {} },
		{ SampleId::QuadTreeCulling,  L"5. 쿼드트리 컬링 (QuadTree Culling)", {} },
		{ SampleId::DistanceLod1,     L"6. 거리 기반 LOD 지형 1 (Distance LOD 1)", {} },
		{ SampleId::DistanceLod2,     L"6-2. 고급 거리 LOD 지형 2 (스티칭 & 지오모핑)", {} },
		{ SampleId::Tessellation,     L"7. 하드웨어 테셀레이션 지형 (Tessellation)", {} },
		{ SampleId::SkyDome,          L"8. 스카이맵 (SkyDome / SkyBox)", {} },
		{ SampleId::PerturbedClouds,  L"9. 동적 왜곡 구름 (Perturbed Clouds)", {} },
		{ SampleId::InfiniteChunks,   L"10. 무한 지형 청크 (Infinite Chunks)", {} },
	};
}
void MenuScene::OnKeyDown(WPARAM key)
{
	switch (key)
	{
	case '1': m_pending = SampleId::FlatGrid;         return;
	case '2': m_pending = SampleId::PerlinNoise;      return;
	case '3': m_pending = SampleId::HeightMap;        return;
	case '4': m_pending = SampleId::TextureSplatting; return;
	case '5': m_pending = SampleId::QuadTreeCulling;  return;
	case '6': m_pending = SampleId::DistanceLod1;     return;
	case '7': m_pending = SampleId::Tessellation;     return;
	case '8': m_pending = SampleId::SkyDome;          return;
	case '9': m_pending = SampleId::PerturbedClouds;  return;
	case '0': m_pending = SampleId::InfiniteChunks;   return;
	// 6-2 번 항목은 숫자 하나로 표현할 수 없어 빼기 키에 배정했다.
	case VK_OEM_MINUS:
	case VK_SUBTRACT:
		m_pending = SampleId::DistanceLod2;
		return;
	// 위아래 화살표로 포커스를 옮기고 엔터로 실행할 수도 있다.
	case VK_UP:
		m_focused = (m_focused + static_cast<int>(m_items.size()) - 1) % static_cast<int>(m_items.size());
		return;
	case VK_DOWN:
		m_focused = (m_focused + 1) % static_cast<int>(m_items.size());
		return;
	case VK_RETURN:
	case VK_SPACE:
		if (m_focused >= 0 && m_focused < static_cast<int>(m_items.size()))
		{
			m_pending = m_items[m_focused].id;
		}
		return;
	default:
		return;
	}
}
void MenuScene::OnMouseMove(int x, int y)
{
	const float fx = static_cast<float>(x);
	const float fy = static_cast<float>(y);
	// 마우스가 올라간 항목을 키보드 포커스와 같은 것으로 취급해 하이라이트를 일치시킨다.
	m_hovered = -1;
	for (int i = 0; i < static_cast<int>(m_items.size()); ++i)
	{
		if (m_items[i].rect.Contains(fx, fy))
		{
			m_hovered = i;
			m_focused = i;
			return;
		}
	}
}
void MenuScene::OnMouseDown(int x, int y)
{
	const float fx = static_cast<float>(x);
	const float fy = static_cast<float>(y);
	for (const MenuItem& item : m_items)
	{
		if (item.rect.Contains(fx, fy))
		{
			m_pending = item.id;
			return;
		}
	}
}
// 선택을 한 번만 소비하게 해서, main 의 루프가 매 프레임 같은 샘플을 다시 여는 것을 막는다.
SampleId MenuScene::ConsumeSelection()
{
	const SampleId selected = m_pending;
	m_pending = SampleId::None;
	if (selected != SampleId::None)
	{
		const int index = IndexOf(selected);
		if (index >= 0)
		{
			m_focused = index;
		}
	}
	return selected;
}
void MenuScene::Render(TextRenderer& text, UINT screenWidth, UINT screenHeight)
{
	UNREFERENCED_PARAMETER(screenHeight);
	const float centerX = static_cast<float>(screenWidth) * 0.5f;
	const float lineHeight = text.GetLineHeight();
	// 제목과 안내문은 각각 가운데 정렬한다.
	text.Draw(m_title, centerX - text.MeasureWidth(m_title) * 0.5f, TitleY, ColorAccent);
	text.Draw(m_guide, centerX - text.MeasureWidth(m_guide) * 0.5f, GuideY, ColorGuide);
	// 항목마다 길이가 달라 각자 가운데 정렬하면 들쭉날쭉해진다.
	// 가장 긴 항목을 기준으로 블록 전체를 가운데 정렬하고, 안에서는 왼쪽 맞춤으로 둔다.
	float widestItem = 0.0f;
	for (const MenuItem& item : m_items)
	{
		widestItem = (std::max)(widestItem, text.MeasureWidth(item.label));
	}
	const float blockLeft = centerX - widestItem * 0.5f;
	float y = ItemsTop;
	for (int i = 0; i < static_cast<int>(m_items.size()); ++i)
	{
		MenuItem& item = m_items[i];
		const float width = text.MeasureWidth(item.label);
		// 히트 테스트용 사각형은 실제로 그려진 글자 영역과 동일하게 갱신한다.
		item.rect = { blockLeft, y, blockLeft + width, y + lineHeight };
		const bool highlighted = (i == m_hovered) || (i == m_focused);
		text.Draw(item.label, blockLeft, y, highlighted ? ColorAccent : ColorNormal);
		y += ItemSpacing;
	}
}
// TextRenderer 는 아틀라스에 구워둔 글자만 그릴 수 있다.
// 메뉴에 쓰이는 문자열을 훑어 필요한 문자를 빠짐없이 모아준다.
std::wstring MenuScene::BuildCharset() const
{
	std::set<wchar_t> unique;
	const auto insertAll = [&unique](const std::wstring& source)
		{
			for (wchar_t ch : source)
			{
				if (ch != L'\n' && ch != L'\r')
				{
					unique.insert(ch);
				}
			}
		};
	insertAll(m_title);
	insertAll(m_guide);
	for (const MenuItem& item : m_items)
	{
		insertAll(item.label);
	}
	// 타이틀바/HUD 에서 쓰는 영숫자와 기호도 미리 확보해 둔다.
	insertAll(L"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
	insertAll(L" .,:;/|()[]<>-_+=*%#@!?'\"");
	insertAll(L"메인 메뉴와이어프레임샘플화질프레임속도현재");
	return std::wstring(unique.begin(), unique.end());
}
const wchar_t* MenuScene::GetSampleName(SampleId id)
{
	switch (id)
	{
	case SampleId::FlatGrid:         return L"Basic Flat Grid";
	case SampleId::PerlinNoise:      return L"Perlin Noise";
	case SampleId::HeightMap:        return L"HeightMap";
	case SampleId::TextureSplatting: return L"Texture Splatting";
	case SampleId::QuadTreeCulling:  return L"QuadTree Culling";
	case SampleId::DistanceLod1:     return L"Distance LOD 1";
	case SampleId::DistanceLod2:     return L"Distance LOD 2";
	case SampleId::Tessellation:     return L"Tessellation";
	case SampleId::SkyDome:          return L"SkyDome";
	case SampleId::PerturbedClouds:  return L"Perturbed Clouds";
	case SampleId::InfiniteChunks:   return L"Infinite Chunks";
	default:                         return L"Main Menu";
	}
}
int MenuScene::IndexOf(SampleId id) const
{
	for (int i = 0; i < static_cast<int>(m_items.size()); ++i)
	{
		if (m_items[i].id == id)
		{
			return i;
		}
	}
	return -1;
}
