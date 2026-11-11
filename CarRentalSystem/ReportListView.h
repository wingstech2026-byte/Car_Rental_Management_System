// =============================================================================
// ReportListView.h
// -----------------------------------------------------------------------------
// ENCAPSULATION of a Win32 ListView in "report" (table) mode. The rest of the
// GUI never sends raw LVM_* messages; it hands over rows and asks for the
// selected row. The class also implements:
//   * sorting by clicking a column header (text or numeric, with sort arrows),
//   * keeping the selection when the rows are refreshed,
//   * per-row background colours through NM_CUSTOMDRAW,
//   * proportional column widths that follow the control's size.
// =============================================================================
#pragma once

#include "Win32Include.h"

#include <commctrl.h>

#include <optional>
#include <string>
#include <vector>

class ReportListView
{
public:
    /// Describes one column.
    struct Column
    {
        std::wstring title;
        int weight = 1;            ///< Relative width.
        bool rightAligned = false; ///< Right-align (money columns).
        bool numericSort = false;  ///< Sort by Cell::sortValue instead of text.
    };

    /// One cell: the text shown and a number used for numeric sorting.
    struct Cell
    {
        std::wstring text;
        long long sortValue = 0;
    };

    /// One row of the table.
    struct Row
    {
        std::wstring key;                    ///< Identifies the row (the plate).
        std::vector<Cell> cells;             ///< One cell per column.
        bool actionable = true;              ///< May the row's main action be used?
        std::optional<COLORREF> background;  ///< Custom background colour.
    };

    ReportListView() = default;
    ReportListView(const ReportListView&) = delete;
    ReportListView& operator=(const ReportListView&) = delete;

    /// Creates the ListView control as a child of parent.
    void create(HWND parent, int controlId, HINSTANCE instance, const std::vector<Column>& columns);

    HWND handle() const noexcept;

    /// Replaces all rows, re-applies the current sort and keeps the selection.
    void setRows(std::vector<Row> rows);

    /// The selected row, or nullptr if nothing is selected.
    const Row* selectedRow() const;

    /// Selects the row with the given key (if present).
    void selectKey(const std::wstring& key);

    /// Sorts by the column; clicking the same column again reverses the order.
    void sortByColumn(int column);

    /// Distributes the available width over the columns according to their weights.
    void fitColumnsToWidth();

    /// Handles NM_CUSTOMDRAW to colour rows. Returns the value for WM_NOTIFY.
    LRESULT handleCustomDraw(NMLVCUSTOMDRAW& draw) const;

private:
    int selectedIndex() const;
    std::wstring selectedKey() const;
    void selectIndex(int index);
    void sortRows();
    void repopulate();
    void updateSortArrows() const;

    HWND m_hwnd = nullptr;
    std::vector<Column> m_columns;
    std::vector<Row> m_rows; ///< Always in the same order as the items in the control.
    int m_sortColumn = -1;   ///< -1 = original order.
    bool m_sortAscending = true;
};
