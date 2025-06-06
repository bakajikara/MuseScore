/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2023 MuseScore Limited
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
#include "tremololayout.h"

#include "global/types/number.h"

#include "dom/chord.h"
#include "dom/stem.h"
#include "dom/tremolotwochord.h"
#include "dom/tremolosinglechord.h"
#include "dom/note.h"
#include "dom/staff.h"
#include "dom/measure.h"

#include "beamtremololayout.h"
#include "chordlayout.h"
#include "stemlayout.h"

using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

void TremoloLayout::layout(TremoloTwoChord* item, const LayoutContext& ctx)
{
    IF_ASSERT_FAILED(item->explicitParent()) {
        return;
    }

    item->setChord1(toChord(item->explicitParent()));

    Note* anchor1 = item->chord1()->up() ? item->chord1()->upNote() : item->chord1()->downNote();
    Stem* stem    = item->chord1()->stem();
    double x, y, h;
    if (stem) {
        x = stem->pos().x() + stem->width() / 2 * (item->chord1()->up() ? -1.0 : 1.0);
        y = stem->pos().y();
        h = stem->length();
    } else {
        // center tremolo above note
        x = anchor1->x() + anchor1->headWidth() * 0.5;
        y = anchor1->y();
        h = (ctx.conf().styleMM(Sid::tremoloNoteSidePadding).val() + item->ldata()->bbox().height()) * item->chord1()->intrinsicMag();
    }

    layoutTwoNotesTremolo(item, ctx, x, y, h, item->spatium());
}

void TremoloLayout::layout(TremoloSingleChord* item, const LayoutContext& ctx)
{
    IF_ASSERT_FAILED(item->explicitParent()) {
        return;
    }

    item->computeShape();      // set bbox
    item->setPath(item->basePath());

    Note* anchor1 = item->chord()->up() ? item->chord()->upNote() : item->chord()->downNote();
    Stem* stem = item->chord()->stem();
    double x, y, h;
    if (stem) {
        x = stem->pos().x() + stem->width() / 2 * (item->chord()->up() ? -1.0 : 1.0);
        y = stem->pos().y();
        h = stem->length();
    } else {
        // center tremolo above note
        x = anchor1->x() + anchor1->headWidth() * 0.5;

        bool hasMirroredNote = false;
        for (Note* n : item->chord()->notes()) {
            if (n->ldata()->mirror.value()) {
                hasMirroredNote = true;
                break;
            }
        }

        if (hasMirroredNote) {
            x = StemLayout::StemLayout::stemPosX(item->chord());
        }

        y = anchor1->y();
        h = (ctx.conf().styleMM(Sid::tremoloNoteSidePadding).val() + item->ldata()->bbox().height()) * item->chord()->intrinsicMag();
    }

    layoutOneNoteTremolo(item, ctx, x, y, h, item->spatium());
}

//---------------------------------------------------------
//   layoutOneNoteTremolo
//---------------------------------------------------------

void TremoloLayout::layoutOneNoteTremolo(TremoloSingleChord* item, const LayoutContext& ctx, double x, double y, double h, double spatium)
{
    const StaffType* staffType = item->staffType();

    if (staffType && staffType->isTabStaff()) {
        x = ChordLayout::centerX(item->chord());
    }

    double staveOffset = item->staffOffsetY();
    bool up = item->chord()->up();
    int upValue = up ? -1 : 1;
    double mag = item->chord()->intrinsicMag();
    spatium *= mag;

    double yOffset = h - ctx.conf().styleMM(Sid::tremoloOutSidePadding).val() * mag;

    int beams = item->chord()->beams();
    if (item->chord()->hook()) {
        // allow for space at the hook side of the stem (yOffset)
        // straight flags and traditional flags have different requirements because of their slopes
        // away from the stem. Straight flags have a shallower slope and a lot more space in general
        // so we can place the trem higher in that case
        bool straightFlags = ctx.conf().styleB(Sid::useStraightNoteFlags);
        if (straightFlags) {
            yOffset -= 0.75 * spatium;
        } else {
            // up-hooks and down-hooks are shaped differently
            yOffset -= up ? 1.5 * spatium : 1.0 * spatium;
        }
        // we need an additional offset for beams > 2 since those beams extend outwards and we don't want to adjust for that
        double beamOffset = straightFlags ? 0.75 : 0.5;
        yOffset -= beams >= 2 ? beamOffset * spatium : 0.0;
    } else if (beams && staffType && !staffType->isSimpleTabStaff()) {
        yOffset -= (beams * (ctx.conf().styleB(Sid::useWideBeams) ? 1.0 : 0.75) - 0.25) * spatium;
    }
    yOffset -= item->isBuzzRoll() && up ? 0.5 * spatium : 0.0;
    yOffset -= up ? 0.0 : item->minHeight() * spatium / mag;
    yOffset *= upValue;
    yOffset += staveOffset;
    y += yOffset;

    if (up) {
        double height = item->isBuzzRoll() ? 0 : item->minHeight();
        double staveHeight = (((item->staff()->lines(item->tick()) - 1) - height) * spatium / mag) + staveOffset;
        y = std::min(y, staveHeight);
    } else {
        y = std::max(y, 0.0);
    }
    item->setPos(x, y);
}

void TremoloLayout::calcIsUp(TremoloTwoChord* item)
{
    if (item->userModified()) {
        bool startChordUp = ChordLayout::isChordPosBelowTrem(item->chord1(), item);
        bool endChordUp = ChordLayout::isChordPosBelowTrem(item->chord2(), item);

        item->setUp(startChordUp || endChordUp);
        item->chord1()->setUp(startChordUp);
        item->chord2()->setUp(endChordUp);
        return;
    }

    int up = 0;
    bool isUp = item->up();
    bool hasVoices = item->chord1()->measure()->hasVoices(item->chord1()->staffIdx(), item->chord1()->tick(),
                                                          item->chord2()->tick() - item->chord1()->tick());
    if (item->chord1()->stemDirection() == DirectionV::AUTO
        && item->chord2()->stemDirection() == DirectionV::AUTO
        && item->chord1()->staffMove() == item->chord2()->staffMove()
        && !hasVoices) {
        std::vector<int> noteDistances;
        for (int distance : item->chord1()->noteDistances()) {
            noteDistances.push_back(distance);
        }
        for (int distance : item->chord2()->noteDistances()) {
            noteDistances.push_back(distance);
        }
        std::sort(noteDistances.begin(), noteDistances.end());
        up = ChordLayout::computeAutoStemDirection(noteDistances);

        isUp = up > 0;
    } else if (item->chord1()->stemDirection() != DirectionV::AUTO) {
        isUp = item->chord1()->stemDirection() == DirectionV::UP;
    } else if (item->chord2()->stemDirection() != DirectionV::AUTO) {
        isUp = item->chord2()->stemDirection() == DirectionV::UP;
    } else if (item->chord1()->staffMove() > 0 || item->chord2()->staffMove() > 0) {
        isUp = false;
    } else if (item->chord1()->staffMove() < 0 || item->chord2()->staffMove() < 0) {
        isUp = true;
    } else if (hasVoices) {
        isUp = item->chord1()->track() % 2 == 0;
    }

    item->setUp(isUp);
    item->chord1()->setUp(item->chord1()->staffMove() == 0 ? isUp : !isUp); // if on a different staff, flip stem dir
    item->chord2()->setUp(item->chord2()->staffMove() == 0 ? isUp : !isUp);
}

//---------------------------------------------------------
//   layoutTwoNotesTremolo
//---------------------------------------------------------

void TremoloLayout::layoutTwoNotesTremolo(TremoloTwoChord* item, const LayoutContext& ctx, double x, double y, double h, double spatium)
{
    UNUSED(x);
    UNUSED(y);
    UNUSED(h);
    UNUSED(spatium);

    TremoloTwoChord::LayoutData* ldata = item->mutldata();

    // make sure both stems are in the same direction
    if (item->chord1()->beam() && item->chord1()->beam() == item->chord2()->beam()) {
        Beam* beam = item->chord1()->beam();
        item->setUp(beam->up());
        item->doSetDirection(beam->direction());
        // stem stuff is already taken care of by the beams
    } else {
        TremoloLayout::calcIsUp(item);

        if (!item->userModified()) {
            // user modified trems will be dealt with later
            ChordLayout::layoutStem(item->chord1(), ctx);
            ChordLayout::layoutStem(item->chord2(), ctx);
        }
    }

    BeamTremoloLayout::setupLData(item, item->mutldata(), ctx);
    item->setStartAnchor(BeamTremoloLayout::chordBeamAnchor(item->ldata(), item->chord1(), ChordBeamAnchorType::Start));
    item->setEndAnchor(BeamTremoloLayout::chordBeamAnchor(item->ldata(), item->chord2(), ChordBeamAnchorType::End));

    // deal with manual adjustments here and return
    PropertyValue val = item->getProperty(Pid::PLACEMENT);
    if (item->userModified()) {
        int idx = item->directionIdx();
        double startY = item->beamFragment().py1[idx];
        double endY = item->beamFragment().py2[idx];
        if (ctx.conf().styleB(Sid::snapCustomBeamsToGrid)) {
            const double quarterSpace = item->EngravingItem::spatium() / 4;
            startY = round(startY / quarterSpace) * quarterSpace;
            endY = round(endY / quarterSpace) * quarterSpace;
        }
        startY += item->pagePos().y();
        endY += item->pagePos().y();
        item->startAnchor().setY(startY);
        item->endAnchor().setY(endY);
        item->mutldata()->setAnchors(item->startAnchor(), item->endAnchor());

        ChordLayout::layoutStem(item->chord1(), ctx);
        ChordLayout::layoutStem(item->chord2(), ctx);

        createBeamSegments(item, ctx);
        return;
    }
    ldata->setPosY(0.);
    std::vector<ChordRest*> chordRests{ item->chord1(), item->chord2() };
    std::vector<BeamBase::NotePosition> notes;
    double mag = 0.0;

    notes.clear();
    for (ChordRest* cr : chordRests) {
        double m = cr->isSmall() ? ctx.conf().styleD(Sid::smallNoteMag) : 1.0;
        mag = std::max(mag, m);
        if (cr->isChord()) {
            Chord* chord = toChord(cr);
            //int i = chord->staffMove();
            //_minMove = std::min(_minMove, i); todo: investigate this
            //_maxMove = std::max(_maxMove, i);

            for (int distance : chord->noteDistances()) {
                notes.push_back(BeamBase::NotePosition(distance, chord->vStaffIdx()));
            }
        }
    }

    std::sort(notes.begin(), notes.end());
    ldata->setMag(mag);
    BeamTremoloLayout::calculateAnchors(item, item->mutldata(), ctx, chordRests, notes);
    item->setStartAnchor(item->ldata()->startAnchor);
    item->setEndAnchor(item->ldata()->endAnchor);

    int idx = item->directionIdx();
    item->beamFragment().py1[idx] = item->startAnchor().y() - item->pagePos().y();
    item->beamFragment().py2[idx] = item->endAnchor().y() - item->pagePos().y();
    createBeamSegments(item, ctx);
}

//---------------------------------------------------------
//   extendedStemLenWithTwoNotesTremolo
//    Goal: To extend stem of one of the chords to make the tremolo less steep
//    Returns a modified pair of stem lengths of two chords
//---------------------------------------------------------

std::pair<double, double> TremoloLayout::extendedStemLenWithTwoNoteTremolo(TremoloTwoChord* tremolo, double stemLen1, double stemLen2)
{
    const double spatium = tremolo->spatium();
    Chord* c1 = tremolo->chord1();
    Chord* c2 = tremolo->chord2();
    Stem* s1 = c1->stem();
    Stem* s2 = c2->stem();
    const double sgn1 = c1->up() ? -1.0 : 1.0;
    const double sgn2 = c2->up() ? -1.0 : 1.0;
    const double stemTipDistance = (s1 && s2) ? (s2->pagePos().y() + stemLen2) - (s1->pagePos().y() + stemLen1)
                                   : (StemLayout::stemPos(c2).y() + (sgn2 * stemLen2)) - (StemLayout::stemPos(c1).y() + (sgn1 * stemLen1));

    // same staff & same direction: extend one of the stems
    if (c1->staffMove() == c2->staffMove() && c1->up() == c2->up()) {
        const bool stem1Higher = stemTipDistance > 0.0;
        if (std::abs(stemTipDistance) > 1.0 * spatium) {
            if ((c1->up() && !stem1Higher) || (!c1->up() && stem1Higher)) {
                return { stemLen1 + (std::abs(stemTipDistance) - spatium), stemLen2 };
            } else {   /* if ((c1->up() && stem1Higher) || (!c1->up() && !stem1Higher)) */
                return { stemLen1, stemLen2 + (std::abs(stemTipDistance) - spatium) };
            }
        }
    }

    return { stemLen1, stemLen2 };
}

void TremoloLayout::createBeamSegments(TremoloTwoChord* item, const LayoutContext& ctx)
{
    // TODO: This should be a style setting, to replace tremoloStrokeLengthMultiplier
    static constexpr double stemGapSp = 1.0;
    const bool defaultStyle = (!item->customStyleApplicable()) || (item->tremoloStyle() == TremoloStyle::DEFAULT);

    item->clearBeamSegments();

    bool _isGrace = item->chord1()->isGrace();
    const PointF pagePos = item->pagePos();
    PointF startAnchor = item->ldata()->startAnchor - PointF(0.0, pagePos.y());
    PointF endAnchor = item->ldata()->endAnchor - PointF(0.0, pagePos.y());

    // inset trem from stems for default style
    const double slope = muse::divide(endAnchor.y() - startAnchor.y(), endAnchor.x() - startAnchor.x(), 0.0);

    double gapSp = stemGapSp;
    if (defaultStyle || item->tremoloStyle() == TremoloStyle::TRADITIONAL_ALTERNATE) {
        // we can eat into the stemGapSp margin if the anchorpoints are sufficiently close together
        double widthSp = (endAnchor.x() - startAnchor.x()) / item->spatium() - (stemGapSp * 2);
        if (!muse::RealIsEqualOrMore(widthSp, 0.6)) {
            // tremolo beam is too short; we can eat into the gap spacing a little
            gapSp = std::max(stemGapSp - ((0.6 - widthSp) * 0.5), 0.4);
        }
    } else {
        gapSp = 0.0;
    }
    BeamSegment* mainStroke = new BeamSegment(item);
    PointF xOffset = PointF(gapSp * item->spatium(), 0);
    PointF yOffset = PointF(0, gapSp * item->spatium() * slope);
    if (item->tremoloStyle() == TremoloStyle::TRADITIONAL_ALTERNATE) {
        mainStroke->line = LineF(startAnchor, endAnchor);
        startAnchor += xOffset;
        endAnchor -= xOffset;
        startAnchor += yOffset;
        endAnchor -= yOffset;
    } else {
        startAnchor += xOffset;
        endAnchor -= xOffset;
        startAnchor += yOffset;
        endAnchor -= yOffset;
        mainStroke->line = LineF(startAnchor, endAnchor);
    }
    mainStroke->level = 0;

    item->beamSegments().push_back(mainStroke);

    LineF line = mainStroke->line;
    double halfWidth = 0.5 * ctx.conf().styleMM(Sid::beamWidth).val();
    double bboxTop = std::min(line.y1(), line.y2()) - halfWidth;
    double bboxBottom = std::max(line.y1(), line.y2()) + halfWidth;

    RectF bbox = RectF(line.x1(), bboxTop, line.x2() - line.x1(), bboxBottom - bboxTop);

    PointF beamOffset = PointF(0., (item->up() ? 1 : -1) * item->spatium() * (ctx.conf().styleB(Sid::useWideBeams) ? 1. : 0.75));
    beamOffset.setY(beamOffset.y() * item->mag() * (_isGrace ? ctx.conf().styleD(Sid::graceNoteMag) : 1.));
    for (int i = 1; i < item->lines(); ++i) {
        BeamSegment* stroke = new BeamSegment(item);
        stroke->level = i;
        stroke->line = LineF(startAnchor + (beamOffset * (double)i), endAnchor + (beamOffset * (double)i));
        item->beamSegments().push_back(stroke);
        bbox.unite(bbox.translated(0., beamOffset.y() * (double)i));
    }
    item->setbbox(bbox);

    // size stems properly
    if (item->chord1()->stem() && item->chord2()->stem()
        && !(item->chord1()->beam() && item->chord1()->beam() == item->chord2()->beam())) {
        // we don't need to do anything if these chords are part of the same beam--their stems are taken care of
        // by the beam layout
        int beamSpacing = ctx.conf().styleB(Sid::useWideBeams) ? 4 : 3;
        for (ChordRest* cr : { item->chord1(), item->chord2() }) {
            Chord* chord = toChord(cr);
            double addition = 0.0;
            if (cr->up() != item->up() && item->lines() > 1) {
                // need to adjust further for beams on the opposite side
                addition += (item->lines() - 1.) * beamSpacing / 4. * item->spatium() * item->mag();
            }
            // calling extendStem with addition 0.0 still sizes the stem to the manually adjusted height of the trem.
            BeamTremoloLayout::extendStem(item->ldata(), chord, addition);
        }
    }
}
