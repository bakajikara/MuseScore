/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "previewmeasure.h"

#include "engraving/dom/staff.h"
#include "engraving/dom/measure.h"
#include "draw/painter.h"
#include "draw/internal/qpainterprovider.h"
#include <QPainter>
#include <QLinearGradient>

using namespace mu::notation;
using namespace mu::engraving;
using namespace muse;
using namespace muse::draw;

void PreviewMeasure::paint(Painter* painter, const NoteInputState& state)
{
    const Score* score = state.staff()->score();
    const Measure* lastMeasure = score->lastMeasure();
    const System* lastSystem = lastMeasure->system();

    if (!lastMeasure) {
        return;
    }

    const Fraction tick = lastMeasure->endTick();
    const double previewWidth = 100;

    for (staff_idx_t staffIdx = 0; staffIdx < score->nstaves(); ++staffIdx) {
        Staff* staff = score->staff(staffIdx);
        SysStaff* sysStaff = lastSystem->staff(staffIdx);

        const int staffLines = staff->lines(tick);
        const double spatium = staff->score()->style().spatium();
        const double lineDist = staff->staffType(tick)->lineDistance().val() * staff->staffMag(tick) * spatium;
        const double lineWidth = lastMeasure->style().styleMM(Sid::staffLineWidth) * staff->staffMag(tick);

        const PointF measurePos = lastMeasure->canvasPos();
        const double staffY = measurePos.y() + sysStaff->y();
        const double startX = measurePos.x() + lastMeasure->width();

        paintStaffLines(painter, PointF(startX, staffY), previewWidth, staffLines, lineDist, lineWidth);
    }
}

void PreviewMeasure::paintStaffLines(Painter* painter, const PointF& pos,
                                     double width, int lines, double lineDist, double lineWidth)
{
    if (lines <= 0 || width <= 0) {
        return;
    }

    QPainterProvider* qpainterProvider = dynamic_cast<QPainterProvider*>(painter->provider().get());
    QPainter* qpainter = qpainterProvider->qpainter();

    QColor lineColor = configuration()->noteInputPreviewColor().toQColor();

    QLinearGradient gradient(pos.x(), pos.y(), pos.x() + width, pos.y());
    gradient.setColorAt(0, lineColor);
    lineColor.setAlpha(0);
    gradient.setColorAt(1, lineColor);

    QBrush brush(gradient);
    qpainter->setBrush(brush);
    qpainter->setPen(Qt::NoPen);

    for (int i = 0; i < lines; ++i) {
        double y = pos.y() + i * lineDist;
        qpainter->drawRect(QRectF(pos.x(), y - lineWidth / 2, width, lineWidth));
    }
}

bool PreviewMeasure::isBeyondScore(const NoteInputState& state, const PointF& mousePos) const
{
    const Score* score = state.staff()->score();
    const Measure* lastMeasure = score->lastMeasure();
    if (!lastMeasure) {
        return false;
    }

    Position pos = Position();
    score->getPosition(&pos, mousePos, state.voice());

    return pos.beyondScore;
}

RectF PreviewMeasure::previewRect(const NoteInputState& state, const PointF& mousePos) const
{
    if (!state.beyondScore() && !isBeyondScore(state, mousePos)) {
        return RectF();
    }

    const Score* score = state.staff()->score();
    const Measure* lastMeasure = score->lastMeasure();
    const System* lastSystem = lastMeasure->system();

    return RectF(lastMeasure->canvasPos().x() + lastMeasure->width(), lastSystem->canvasBoundingRect().y(),
                 100, lastSystem->canvasBoundingRect().height());
}
