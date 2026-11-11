// =============================================================================
// ReportListView.cpp
// =============================================================================
#include "ReportListView.h"

#include "GuiUtils.h"
#include "TextUtils.h"

#include <uxtheme.h>

#include <algorithm>
#include <utility>

void ReportListView::create(HWND parent, int controlId, HINSTANCE instance, const std::vector<Column>& columns)
{
    m_columns = columns;
    m_hwnd = Gui::createChild(parent, instance, WC_LISTVIEWW, L"",
                              WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                              WS_EX_CLIENTEDGE, controlId);

    const DWORD extendedStyle = LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP;
    SendMessageW(m_hwnd, LVM_SETEXTENDEDLISTVIEWSTYLE, extendedStyle, extendedStyle);
    SetWindowTheme(m_hwnd, L"Explorer", nullptr); // modern Explorer look (selection, header)

    for (std::size_t index = 0; index < m_columns.size(); ++index)
    {
        LVCOLUMNW column{};
        column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT | LVCF_SUBITEM;
        column.fmt = m_columns[index].rightAligned ? LVCFMT_RIGHT : LVCFMT_LEFT;
        column.cx = 100;
        column.pszText = m_columns[index].title.data();
        column.iSubItem = static_cast<int>(index);
        SendMessageW(m_hwnd, LVM_INSERTCOLUMNW, index, reinterpret_cast<LPARAM>(&column));
    }
}

HWND ReportListView::handle() const noexcept
{
    return m_hwnd;
}

void ReportListView::setRows(std::vector<Row> rows)
{
    const std::wstring previousKey = selectedKey();
    m_rows = std::move(rows);
    sortRows();
    repopulate();
    if (!previousKey.empty())
    {
        selectKey(previousKey);
    }
    fitColumnsToWidth(); // a vertical scroll bar may have appeared or disappeared
}

const ReportListView::Row* ReportListView::selectedRow() const
{
    const int index = selectedIndex();
    if (index < 0 || static_cast<std::size_t>(index) >= m_rows.size())
    {
        return nullptr;
    }
    return &m_rows[static_cast<std::size_t>(index)];
}

void ReportListView::selectKey(const std::wstring& key)
{
    for (std::size_t index = 0; index < m_rows.size(); ++index)
    {
        if (m_rows[index].key == key)
        {
            selectIndex(static_cast<int>(index));
            return;
        }
    }
}

void ReportListView::sortByColumn(int column)
{
    if (column < 0 || static_cast<std::size_t>(column) >= m_columns.size())
    {
        return;
    }
    if (column == m_sortColumn)
    {
        m_sortAscending = !m_sortAscending;
    }
    else
    {
        m_sortColumn = column;
        m_sortAscending = true;
    }

    const std::wstring previousKey = selectedKey();
    sortRows();
    repopulate();
    if (!previousKey.empty())
    {
        selectKey(previousKey);
    }
    updateSortArrows();
}

void ReportListView::fitColumnsToWidth()
{
    if (m_hwnd == nullptr || m_columns.empty())
    {
        return;
    }
    RECT client{};
    GetClientRect(m_hwnd, &client);
    const int width = client.right - client.left - 1; // -1 avoids a horizontal scroll bar
    if (width <= 0)
    {
        return;
    }

    int totalWeight = 0;
    for (const Column& column : m_columns)
    {
        totalWeight += std::max(column.weight, 1);
    }

    int used = 0;
    const int lastIndex = static_cast<int>(m_columns.size()) - 1;
    for (int index = 0; index <= lastIndex; ++index)
    {
        const int weight = std::max(m_columns[static_cast<std::size_t>(index)].weight, 1);
        const int columnWidth = (index == lastIndex) ? width - used : MulDiv(width, weight, totalWeight);
        used += columnWidth;
        SendMessageW(m_hwnd, LVM_SETCOLUMNWIDTH, static_cast<WPARAM>(index), MAKELPARAM(std::max(columnWidth, 0), 0));
    }
}

LRESULT ReportListView::handleCustomDraw(NMLVCUSTOMDRAW& draw) const
{
    switch (draw.nmcd.dwDrawStage)
    {
    case CDDS_PREPAINT:
        return CDRF_NOTIFYITEMDRAW; // ask for a notification for every item

    case CDDS_ITEMPREPAINT:
    {
        const std::size_t index = static_cast<std::size_t>(draw.nmcd.dwItemSpec);
        if (index < m_rows.size() && m_rows[index].background.has_value())
        {
            draw.clrTextBk = *m_rows[index].background;
            draw.clrText = GetSysColor(COLOR_WINDOWTEXT);
            return CDRF_NEWFONT;
        }
        return CDRF_DODEFAULT;
    }

    default:
        return CDRF_DODEFAULT;
    }
}

// ---- Private helpers --------------------------------------------------------------

int ReportListView::selectedIndex() const
{
    if (m_hwnd == nullptr)
    {
        return -1;
    }
    return static_cast<int>(SendMessageW(m_hwnd, LVM_GETNEXTITEM, static_cast<WPARAM>(-1), LVNI_SELECTED));
}

std::wstring ReportListView::selectedKey() const
{
    const Row* row = selectedRow();
    return row != nullptr ? row->key : std::wstring();
}

void ReportListView::selectIndex(int index)
{
    LVITEMW state{};
    state.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
    state.state = LVIS_SELECTED | LVIS_FOCUSED;
    SendMessageW(m_hwnd, LVM_SETITEMSTATE, static_cast<WPARAM>(index), reinterpret_cast<LPARAM>(&state));
    SendMessageW(m_hwnd, LVM_ENSUREVISIBLE, static_cast<WPARAM>(index), FALSE);
}

void ReportListView::sortRows()
{
    if (m_sortColumn < 0)
    {
        return;
    }
    const std::size_t column = static_cast<std::size_t>(m_sortColumn);
    const bool numeric = m_columns[column].numericSort;
    const bool ascending = m_sortAscending;

    std::stable_sort(m_rows.begin(), m_rows.end(), [column, numeric, ascending](const Row& left, const Row& right) {
        if (column >= left.cells.size() || column >= right.cells.size())
        {
            return false;
        }
        const Cell& a = left.cells[column];
        const Cell& b = right.cells[column];
        int comparison = 0;
        if (numeric)
        {
            comparison = (a.sortValue < b.sortValue) ? -1 : (a.sortValue > b.sortValue ? 1 : 0);
        }
        else
        {
            comparison = TextUtils::compareIgnoreCase(a.text, b.text);
        }
        return ascending ? comparison < 0 : comparison > 0;
    });
}

void ReportListView::repopulate()
{
    SendMessageW(m_hwnd, WM_SETREDRAW, FALSE, 0);
    SendMessageW(m_hwnd, LVM_DELETEALLITEMS, 0, 0);

    for (std::size_t rowIndex = 0; rowIndex < m_rows.size(); ++rowIndex)
    {
        Row& row = m_rows[rowIndex];
        const std::size_t cellCount = std::min(row.cells.size(), m_columns.size());
        for (std::size_t columnIndex = 0; columnIndex < cellCount; ++columnIndex)
        {
            LVITEMW item{};
            item.mask = LVIF_TEXT;
            item.iItem = static_cast<int>(rowIndex);
            item.iSubItem = static_cast<int>(columnIndex);
            item.pszText = row.cells[columnIndex].text.data();
            if (columnIndex == 0)
            {
                SendMessageW(m_hwnd, LVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&item));
            }
            else
            {
                SendMessageW(m_hwnd, LVM_SETITEMTEXTW, rowIndex, reinterpret_cast<LPARAM>(&item));
            }
        }
    }

    SendMessageW(m_hwnd, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void ReportListView::updateSortArrows() const
{
    const HWND header = reinterpret_cast<HWND>(SendMessageW(m_hwnd, LVM_GETHEADER, 0, 0));
    if (header == nullptr)
    {
        return;
    }
    for (int index = 0; index < static_cast<int>(m_columns.size()); ++index)
    {
        HDITEMW item{};
        item.mask = HDI_FORMAT;
        SendMessageW(header, HDM_GETITEMW, static_cast<WPARAM>(index), reinterpret_cast<LPARAM>(&item));
        item.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
        if (index == m_sortColumn)
        {
            item.fmt |= m_sortAscending ? HDF_SORTUP : HDF_SORTDOWN;
        }
        SendMessageW(header, HDM_SETITEMW, static_cast<WPARAM>(index), reinterpret_cast<LPARAM>(&item));
    }
}
