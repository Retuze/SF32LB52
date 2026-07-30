#pragma once
#include "framework/view/view_group.hpp"

namespace litho {

// Flat row-major grid — one ViewGroup, no nested LinearLayouts.
// Children fill cells left→right, top→bottom. No span in v1.
class GridLayout : public ViewGroup {
public:
    static constexpr uint8_t kMaxColumns = 16;

    explicit GridLayout(uint8_t columnCount = 1) {
        setColumnCount(columnCount);
    }

    void setColumnCount(uint8_t n) {
        if (n < 1) n = 1;
        if (n > kMaxColumns) n = kMaxColumns;
        if (n == mColumns) return;
        mColumns = n;
        requestLayout();
    }
    uint8_t columnCount() const { return mColumns; }

    void setHorizontalGap(int16_t g) {
        if (g == mHGap) return;
        mHGap = g;
        requestLayout();
    }
    void setVerticalGap(int16_t g) {
        if (g == mVGap) return;
        mVGap = g;
        requestLayout();
    }
    void setGap(int16_t h, int16_t v) {
        if (h == mHGap && v == mVGap) return;
        mHGap = h;
        mVGap = v;
        requestLayout();
    }
    int16_t horizontalGap() const { return mHGap; }
    int16_t verticalGap() const { return mVGap; }

    // When true and width is EXACTLY: equal column widths fill the content box.
    // When false: each column is max(child widths) in that column (WRAP).
    void setStretchColumns(bool stretch) {
        if (stretch == mStretchColumns) return;
        mStretchColumns = stretch;
        requestLayout();
    }
    bool stretchColumns() const { return mStretchColumns; }

protected:
    void onMeasure(int32_t widthMeasureSpec, int32_t heightMeasureSpec) override {
        const int widthMode  = MeasureSpec::getMode(widthMeasureSpec);
        const int widthSize  = MeasureSpec::getSize(widthMeasureSpec);
        const int cols = mColumns;

        const int vis = visibleChildCount();
        const int rows = (vis == 0) ? 0 : (vis + cols - 1) / cols;

        int colW[kMaxColumns];
        for (int c = 0; c < cols; c++) colW[c] = 0;

        const bool stretch = mStretchColumns && widthMode == MeasureSpec::EXACTLY && cols > 0;
        int cellW = 0;
        if (stretch) {
            const int inner = widthSize - insetHorizontal();
            const int gaps  = (cols > 1) ? mHGap * (cols - 1) : 0;
            cellW = (inner > gaps) ? (inner - gaps) / cols : 0;
            for (int c = 0; c < cols; c++) colW[c] = cellW;
        }

        // Measure children; track column widths (WRAP) and per-row heights.
        int totalH = insetVertical();
        int visIdx = 0;
        int rowH = 0;

        for (uint16_t i = 0; i < childCount(); i++) {
            View* child = childAt(i);
            if (!child || !child->visible()) continue;

            const int col = visIdx % cols;
            const int row = visIdx / cols;
            if (col == 0) {
                if (row > 0) {
                    totalH += rowH + mVGap;
                    rowH = 0;
                }
            }

            litho::LayoutParams* base = child->layoutParams();
            int ml = base ? base->marginL : 0;
            int mt = base ? base->marginT : 0;
            int mr = base ? base->marginR : 0;
            int mb = base ? base->marginB : 0;
            int lpW = base ? base->width  : LayoutParams::WRAP_CONTENT;
            int lpH = base ? base->height : LayoutParams::WRAP_CONTENT;

            int32_t childWSpec;
            if (stretch) {
                const int avail = cellW - ml - mr;
                if (lpW == LayoutParams::MATCH_PARENT || lpW == LayoutParams::WRAP_CONTENT) {
                    childWSpec = MeasureSpec::make(
                        avail > 0 ? avail : 0,
                        lpW == LayoutParams::MATCH_PARENT ? MeasureSpec::EXACTLY
                                                          : MeasureSpec::AT_MOST);
                } else {
                    childWSpec = getChildMeasureSpec(
                        MeasureSpec::make(cellW, MeasureSpec::EXACTLY), ml + mr, lpW);
                }
            } else {
                childWSpec = getChildMeasureSpec(
                    widthMeasureSpec, insetHorizontal() + ml + mr, lpW);
            }

            child->measure(
                childWSpec,
                getChildMeasureSpec(heightMeasureSpec, insetVertical() + mt + mb, lpH));

            const int cw = child->measuredWidth() + ml + mr;
            const int ch = child->measuredHeight() + mt + mb;
            if (!stretch && cw > colW[col]) colW[col] = cw;
            if (ch > rowH) rowH = ch;
            visIdx++;
        }
        if (vis > 0) totalH += rowH;

        int totalW = insetHorizontal();
        if (stretch) {
            totalW = widthSize;
        } else {
            for (int c = 0; c < cols; c++) {
                totalW += colW[c];
                if (c + 1 < cols) totalW += mHGap;
            }
        }

        if (totalW < getSuggestedMinimumWidth())  totalW = getSuggestedMinimumWidth();
        if (totalH < getSuggestedMinimumHeight()) totalH = getSuggestedMinimumHeight();

        setMeasuredDimension(
            MeasureSpec::resolveSize(totalW, widthMeasureSpec),
            MeasureSpec::resolveSize(totalH, heightMeasureSpec));

        (void)rows;
    }

    void onLayout(bool changed, int left, int top, int right, int bottom) override {
        (void)changed; (void)top; (void)left;
        const int parentW = right - left;
        const int cols = mColumns;
        const int boxL = insetLeft();
        const int boxT = insetTop();
        const int innerW = parentW - insetHorizontal();

        int colW[kMaxColumns];
        for (int c = 0; c < cols; c++) colW[c] = 0;

        const int vis = visibleChildCount();
        bool stretch = mStretchColumns && cols > 0;
        // Prefer equal columns whenever we have a concrete content width.
        if (stretch) {
            const int gaps = (cols > 1) ? mHGap * (cols - 1) : 0;
            int cellW = (innerW > gaps) ? (innerW - gaps) / cols : 0;
            for (int c = 0; c < cols; c++) colW[c] = cellW;
        } else {
            int visIdx = 0;
            for (uint16_t i = 0; i < childCount(); i++) {
                View* child = childAt(i);
                if (!child || !child->visible()) continue;
                const int col = visIdx % cols;
                litho::LayoutParams* base = child->layoutParams();
                int ml = base ? base->marginL : 0;
                int mr = base ? base->marginR : 0;
                int cw = child->measuredWidth() + ml + mr;
                if (cw > colW[col]) colW[col] = cw;
                visIdx++;
            }
        }

        int colX[kMaxColumns];
        colX[0] = boxL;
        for (int c = 1; c < cols; c++)
            colX[c] = colX[c - 1] + colW[c - 1] + mHGap;

        int visIdx = 0;
        int childTop = boxT;
        int rowH = 0;
        int rowStart = 0; // visIdx at start of current row

        auto layoutRow = [&](int rowBegin, int rowEnd, int y, int rh) {
            for (int vi = rowBegin; vi < rowEnd; vi++) {
                View* child = visibleChildAt(vi);
                if (!child) continue;
                const int col = vi % cols;
                litho::LayoutParams* base = child->layoutParams();
                int ml = base ? base->marginL : 0;
                int mt = base ? base->marginT : 0;
                int mr = base ? base->marginR : 0;
                int mb = base ? base->marginB : 0;
                int g  = base ? base->gravity : (Gravity::TOP | Gravity::LEFT);

                int cw = child->measuredWidth();
                int ch = child->measuredHeight();
                const int cellL = colX[col];
                const int cellW = colW[col];
                const int availW = cellW - ml - mr;
                const int availH = rh - mt - mb;

                int cl = cellL + ml;
                int ct = y + mt;
                if (g & Gravity::CENTER_H) cl = cellL + ml + (availW - cw) / 2;
                else if (g & Gravity::RIGHT) cl = cellL + cellW - mr - cw;
                if (g & Gravity::CENTER_V) ct = y + mt + (availH - ch) / 2;
                else if (g & Gravity::BOTTOM) ct = y + rh - mb - ch;

                child->layout(cl, ct, cl + cw, ct + ch);
            }
        };

        for (uint16_t i = 0; i < childCount(); i++) {
            View* child = childAt(i);
            if (!child || !child->visible()) continue;

            const int col = visIdx % cols;
            if (col == 0 && visIdx > 0) {
                layoutRow(rowStart, visIdx, childTop, rowH);
                childTop += rowH + mVGap;
                rowH = 0;
                rowStart = visIdx;
            }

            litho::LayoutParams* base = child->layoutParams();
            int mt = base ? base->marginT : 0;
            int mb = base ? base->marginB : 0;
            int ch = child->measuredHeight() + mt + mb;
            if (ch > rowH) rowH = ch;
            visIdx++;
        }
        if (visIdx > rowStart)
            layoutRow(rowStart, visIdx, childTop, rowH);

        (void)bottom;
        (void)vis;
    }

private:
    int visibleChildCount() const {
        int n = 0;
        for (uint16_t i = 0; i < childCount(); i++) {
            View* c = childAt(i);
            if (c && c->visible()) n++;
        }
        return n;
    }

    View* visibleChildAt(int visIndex) const {
        int n = 0;
        for (uint16_t i = 0; i < childCount(); i++) {
            View* c = childAt(i);
            if (!c || !c->visible()) continue;
            if (n == visIndex) return c;
            n++;
        }
        return nullptr;
    }

    uint8_t mColumns        = 1;
    int16_t mHGap           = 0;
    int16_t mVGap           = 0;
    bool    mStretchColumns = false;
};

} // namespace litho
