/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrPathUtils.h"

#include "GrTypes.h"
#include "SkGeometry.h"

SkScalar GrPathUtils::scaleToleranceToSrc(SkScalar devTol,
                                          const SkMatrix& viewM,
                                          const SkRect& pathBounds) {
    // In order to tesselate the path we get a bound on how much the matrix can
    // scale when mapping to screen coordinates.
    SkScalar stretch = viewM.getMaxScale();
    SkScalar srcTol = devTol;

    if (stretch < 0) {
        // take worst case mapRadius amoung four corners.
        // (less than perfect)
        for (int i = 0; i < 4; ++i) {
            SkMatrix mat;
            mat.setTranslate((i % 2) ? pathBounds.fLeft : pathBounds.fRight,
                             (i < 2) ? pathBounds.fTop : pathBounds.fBottom);
            mat.postConcat(viewM);
            stretch = SkMaxScalar(stretch, mat.mapRadius(SK_Scalar1));
        }
    }
    srcTol = SkScalarDiv(srcTol, stretch);
    return srcTol;
}

static const int MAX_POINTS_PER_CURVE = 1 << 10;
static const SkScalar gMinCurveTol = 0.0001f;

uint32_t GrPathUtils::quadraticPointCount(const SkPoint points[],
                                          SkScalar tol) {
    if (tol < gMinCurveTol) {
        tol = gMinCurveTol;
    }
    SkASSERT(tol > 0);

    SkScalar d = points[1].distanceToLineSegmentBetween(points[0], points[2]);
    if (d <= tol) {
        return 1;
    } else {
        // Each time we subdivide, d should be cut in 4. So we need to
        // subdivide x = log4(d/tol) times. x subdivisions creates 2^(x)
        // points.
        // 2^(log4(x)) = sqrt(x);
        int temp = SkScalarCeilToInt(SkScalarSqrt(SkScalarDiv(d, tol)));
        int pow2 = GrNextPow2(temp);
        // Because of NaNs & INFs we can wind up with a degenerate temp
        // such that pow2 comes out negative. Also, our point generator
        // will always output at least one pt.
        if (pow2 < 1) {
            pow2 = 1;
        }
        return SkTMin(pow2, MAX_POINTS_PER_CURVE);
    }
}

uint32_t GrPathUtils::generateQuadraticPoints(const SkPoint& p0,
                                              const SkPoint& p1,
                                              const SkPoint& p2,
                                              SkScalar tolSqd,
                                              SkPoint** points,
                                              uint32_t pointsLeft) {
    if (pointsLeft < 2 ||
        (p1.distanceToLineSegmentBetweenSqd(p0, p2)) < tolSqd) {
        (*points)[0] = p2;
        *points += 1;
        return 1;
    }

    SkPoint q[] = {
        { SkScalarAve(p0.fX, p1.fX), SkScalarAve(p0.fY, p1.fY) },
        { SkScalarAve(p1.fX, p2.fX), SkScalarAve(p1.fY, p2.fY) },
    };
    SkPoint r = { SkScalarAve(q[0].fX, q[1].fX), SkScalarAve(q[0].fY, q[1].fY) };

    pointsLeft >>= 1;
    uint32_t a = generateQuadraticPoints(p0, q[0], r, tolSqd, points, pointsLeft);
    uint32_t b = generateQuadraticPoints(r, q[1], p2, tolSqd, points, pointsLeft);
    return a + b;
}

uint32_t GrPathUtils::generateFanQuadraticPoints(const SkPoint& p0,
                                                 const SkPoint& p1,
                                                 const SkPoint& p2,
                                                 const SkPoint& pivot,
                                                 SkScalar tolSqd,
                                                 SkPoint** points,
                                                 uint32_t pointsLeft) {
    if (pointsLeft < 2 ||
        (p1.distanceToLineSegmentBetweenSqd(p0, p2)) < tolSqd) {
        (*points)[0] = p0;
        (*points)[1] = pivot;
        (*points)[2] = p2;
        *points += 3;
        return 1;
    }

    SkPoint q[] = {
        { SkScalarAve(p0.fX, p1.fX), SkScalarAve(p0.fY, p1.fY) },
        { SkScalarAve(p1.fX, p2.fX), SkScalarAve(p1.fY, p2.fY) },
    };
    SkPoint r = { SkScalarAve(q[0].fX, q[1].fX), SkScalarAve(q[0].fY, q[1].fY) };

    pointsLeft >>= 1;
    uint32_t a = generateFanQuadraticPoints(p0, q[0], r, pivot, tolSqd, points, pointsLeft);
    uint32_t b = generateFanQuadraticPoints(r, q[1], p2, pivot, tolSqd, points, pointsLeft);
    return a + b;
}

uint32_t GrPathUtils::cubicPointCount(const SkPoint points[],
                                           SkScalar tol) {
    if (tol < gMinCurveTol) {
        tol = gMinCurveTol;
    }
    SkASSERT(tol > 0);

    SkScalar d = SkTMax(
        points[1].distanceToLineSegmentBetweenSqd(points[0], points[3]),
        points[2].distanceToLineSegmentBetweenSqd(points[0], points[3]));
    d = SkScalarSqrt(d);
    if (d <= tol) {
        return 1;
    } else {
        int temp = SkScalarCeilToInt(SkScalarSqrt(SkScalarDiv(d, tol)));
        int pow2 = GrNextPow2(temp);
        // Because of NaNs & INFs we can wind up with a degenerate temp
        // such that pow2 comes out negative. Also, our point generator
        // will always output at least one pt.
        if (pow2 < 1) {
            pow2 = 1;
        }
        return SkTMin(pow2, MAX_POINTS_PER_CURVE);
    }
}

uint32_t GrPathUtils::generateShapedQuadraticPoints(
                                       const SkPoint outerPts[],
                                       const SkPoint innerPts[],
                                       SkScalar tolSqd,
                                       SkPoint** points,
                                       uint32_t outerPointsLeft,
                                       uint32_t innerPointsLeft)
{
    bool outerIsLine = false;
    bool innerIsLine = false;
    SkPoint outer[3];
    SkPoint inner[3];

    uint32_t a, b;

    if (outerPointsLeft < 2 ||
        (outerPts[1].distanceToLineSegmentBetweenSqd(outerPts[0], outerPts[2])) < tolSqd) {
        outerIsLine = true;
    }

    if (innerPointsLeft < 2 ||
        (innerPts[1].distanceToLineSegmentBetweenSqd(innerPts[0], innerPts[2])) < tolSqd) {
        innerIsLine = true;
    }

    if (innerIsLine == true && outerIsLine == true) {

        (*points)[0] = outerPts[0];
        (*points)[1] = outerPts[2];
        (*points)[2] = innerPts[2];
        (*points)[3] = outerPts[0];
        (*points)[4] = innerPts[2];
        (*points)[5] = innerPts[0];
        *points += 6;
        return 1;
    }
    else if (outerIsLine == false && innerIsLine == true) {
        SkPoint q[] = {
            { SkScalarAve(outerPts[0].fX, outerPts[1].fX),
              SkScalarAve(outerPts[0].fY, outerPts[1].fY) },
            { SkScalarAve(outerPts[1].fX, outerPts[2].fX),
              SkScalarAve(outerPts[1].fY, outerPts[2].fY) },
        };
        SkPoint r = { SkScalarAve(q[0].fX, q[1].fX), SkScalarAve(q[0].fY, q[1].fY) };

        outerPointsLeft >>= 1;
        outer[0] = outerPts[0];
        outer[1] = q[0];
        outer[2] = r;
        inner[0] = inner[1] = inner[2] = innerPts[0];
        a = generateShapedQuadraticPoints(outer, inner,
                                          tolSqd, points,
                                          outerPointsLeft, 1);

        outer[0] = r;
        outer[1] = q[1];
        outer[2] = outerPts[2];
        inner[0] = inner[1] = inner[2] = innerPts[2];
        b = generateShapedQuadraticPoints(outer, inner,
                                          tolSqd, points,
                                          outerPointsLeft, 1);
        
        (*points)[0] = innerPts[0];
        (*points)[1] = r;
        (*points)[2] = innerPts[2];
        *points += 3;
    }
    else if (outerIsLine == true && innerIsLine == false) {
        SkPoint q[] = {
            { SkScalarAve(innerPts[0].fX, innerPts[1].fX),
              SkScalarAve(innerPts[0].fY, innerPts[1].fY) },
            { SkScalarAve(innerPts[1].fX, innerPts[2].fX),
              SkScalarAve(innerPts[1].fY, innerPts[2].fY) },
        };
        SkPoint r = { SkScalarAve(q[0].fX, q[1].fX), SkScalarAve(q[0].fY, q[1].fY) };

        innerPointsLeft >>= 1;
        inner[0] = innerPts[0];
        inner[1] = q[0];
        inner[2] = r;
        outer[0] = outer[1] = outer[2] = outerPts[0];
        a = generateShapedQuadraticPoints(outer, inner,
                                          tolSqd, points,
                                          1, innerPointsLeft);

        inner[0] = r;
        inner[1] = q[1];
        inner[2] = innerPts[2];
        outer[0] = outer[1] = outer[2] = outerPts[2];
        b = generateShapedQuadraticPoints(outer, inner,
                                          tolSqd, points,
                                          1, innerPointsLeft);
        
        (*points)[0] = outerPts[0];
        (*points)[1] = r;
        (*points)[2] = outerPts[2];
        *points += 3;
    }
    else {
        SkPoint outerQ[] = {
            { SkScalarAve(outerPts[0].fX, outerPts[1].fX),
              SkScalarAve(outerPts[0].fY, outerPts[1].fY) },
            { SkScalarAve(outerPts[1].fX, outerPts[2].fX),
              SkScalarAve(outerPts[1].fY, outerPts[2].fY) },
        };
        SkPoint outerR = { SkScalarAve(outerQ[0].fX, outerQ[1].fX),
                           SkScalarAve(outerQ[0].fY, outerQ[1].fY) };

        SkPoint innerQ[] = {
            { SkScalarAve(innerPts[0].fX, innerPts[1].fX),
              SkScalarAve(innerPts[0].fY, innerPts[1].fY) },
            { SkScalarAve(innerPts[1].fX, innerPts[2].fX),
              SkScalarAve(innerPts[1].fY, innerPts[2].fY) },
        };
        SkPoint innerR = { SkScalarAve(innerQ[0].fX, innerQ[1].fX),
                           SkScalarAve(innerQ[0].fY, innerQ[1].fY) };

        innerPointsLeft >>= 1;
        outerPointsLeft >>= 1;

        outer[0] = outerPts[0];
        outer[1] = outerQ[0];
        outer[2] = outerR;
        inner[0] = innerPts[0];
        inner[1] = innerQ[0];
        inner[2] = innerR;
        a = generateShapedQuadraticPoints(outer, inner,
                                          tolSqd, points,
                                          outerPointsLeft, innerPointsLeft);

        outer[0] = outerR;
        outer[1] = outerQ[1];
        outer[2] = outerPts[2];
        inner[0] = innerR;
        inner[1] = innerQ[1];
        inner[2] = innerPts[2];
        b = generateShapedQuadraticPoints(outer, inner,
                                          tolSqd, points,
                                          outerPointsLeft, innerPointsLeft);
    }

    return a + b;
}

uint32_t GrPathUtils::generateCubicPoints(const SkPoint& p0,
                                          const SkPoint& p1,
                                          const SkPoint& p2,
                                          const SkPoint& p3,
                                          SkScalar tolSqd,
                                          SkPoint** points,
                                          uint32_t pointsLeft) {
    if (pointsLeft < 2 ||
        (p1.distanceToLineSegmentBetweenSqd(p0, p3) < tolSqd &&
         p2.distanceToLineSegmentBetweenSqd(p0, p3) < tolSqd)) {
            (*points)[0] = p3;
            *points += 1;
            return 1;
        }
    SkPoint q[] = {
        { SkScalarAve(p0.fX, p1.fX), SkScalarAve(p0.fY, p1.fY) },
        { SkScalarAve(p1.fX, p2.fX), SkScalarAve(p1.fY, p2.fY) },
        { SkScalarAve(p2.fX, p3.fX), SkScalarAve(p2.fY, p3.fY) }
    };
    SkPoint r[] = {
        { SkScalarAve(q[0].fX, q[1].fX), SkScalarAve(q[0].fY, q[1].fY) },
        { SkScalarAve(q[1].fX, q[2].fX), SkScalarAve(q[1].fY, q[2].fY) }
    };
    SkPoint s = { SkScalarAve(r[0].fX, r[1].fX), SkScalarAve(r[0].fY, r[1].fY) };
    pointsLeft >>= 1;
    uint32_t a = generateCubicPoints(p0, q[0], r[0], s, tolSqd, points, pointsLeft);
    uint32_t b = generateCubicPoints(s, r[1], q[2], p3, tolSqd, points, pointsLeft);
    return a + b;
}

uint32_t GrPathUtils::generateShapedCubicPoints(
                                   const SkPoint outerPts[],
                                   const SkPoint innerPts[],
                                   SkScalar tolSqd,
                                   SkPoint** points,
                                   uint32_t outerPointsLeft,
                                   uint32_t innerPointsLeft)
{
    bool outerIsLine = false;
    bool innerIsLine = false;
    uint32_t a, b;
    SkPoint outer[4];
    SkPoint inner[4];

    if (outerPointsLeft < 2 ||
        (outerPts[1].distanceToLineSegmentBetweenSqd(outerPts[0], outerPts[3]) < tolSqd &&
         outerPts[2].distanceToLineSegmentBetweenSqd(outerPts[0], outerPts[3]) < tolSqd)) {
       //     (*points)[0] = p3;
       //     *points += 1;
       //     return 1;
        outerIsLine = true;
    }

    if (innerPointsLeft < 2 ||
        (innerPts[1].distanceToLineSegmentBetweenSqd(innerPts[0], innerPts[3]) < tolSqd &&
         innerPts[2].distanceToLineSegmentBetweenSqd(innerPts[0], innerPts[3]) < tolSqd)) {
       //     (*points)[0] = p3;
       //     *points += 1;
       //     return 1;
        innerIsLine = true;
    }

    if (innerIsLine == true && outerIsLine == true) {
        (*points)[0] = outerPts[0];
        (*points)[1] = outerPts[3];
        (*points)[2] = innerPts[3];
        (*points)[3] = outerPts[0];
        (*points)[4] = innerPts[3];
        (*points)[5] = innerPts[0];
        *points += 6;
        return 1;
    }
    else if (innerIsLine == true && outerIsLine == false) {
        SkPoint q[] = {
            { SkScalarAve(outerPts[0].fX, outerPts[1].fX),
              SkScalarAve(outerPts[0].fY, outerPts[1].fY) },
            { SkScalarAve(outerPts[1].fX, outerPts[2].fX),
              SkScalarAve(outerPts[1].fY, outerPts[2].fY) },
            { SkScalarAve(outerPts[2].fX, outerPts[3].fX),
              SkScalarAve(outerPts[2].fY, outerPts[3].fY) }
        };
        SkPoint r[] = {
            { SkScalarAve(q[0].fX, q[1].fX),
              SkScalarAve(q[0].fY, q[1].fY) },
            { SkScalarAve(q[1].fX, q[2].fX),
              SkScalarAve(q[1].fY, q[2].fY) }
        };
        SkPoint s = { SkScalarAve(r[0].fX, r[1].fX), SkScalarAve(r[0].fY, r[1].fY) };
        outerPointsLeft >>= 1;
        outer[0] = outerPts[0];
        outer[1] = q[0];
        outer[2] = r[0];
        outer[3] = s;
        inner[0] = inner[1] = inner[2] = inner[3] = innerPts[0];
        a = generateShapedCubicPoints(outer, inner, tolSqd, points,
                                      outerPointsLeft, 1);
        outer[0] = s;
        outer[1] = r[1];
        outer[2] = q[2];
        outer[3] = outerPts[3];
        inner[0] = inner[1] = inner[2] = inner[3] = innerPts[3];
        b = generateShapedCubicPoints(outer, inner, tolSqd, points,
                                      outerPointsLeft, 1);
        (*points)[0] = innerPts[0];
        (*points)[1] = s;
        (*points)[2] = innerPts[3];
        *points += 3;
    }
    else if (outerIsLine == true && innerIsLine == false) {
        SkPoint q[] = {
            { SkScalarAve(innerPts[0].fX, innerPts[1].fX),
              SkScalarAve(innerPts[0].fY, innerPts[1].fY) },
            { SkScalarAve(innerPts[1].fX, innerPts[2].fX),
              SkScalarAve(innerPts[1].fY, innerPts[2].fY) },
            { SkScalarAve(innerPts[2].fX, innerPts[3].fX),
              SkScalarAve(innerPts[2].fY, innerPts[3].fY) }
        };
        SkPoint r[] = {
            { SkScalarAve(q[0].fX, q[1].fX),
              SkScalarAve(q[0].fY, q[1].fY) },
            { SkScalarAve(q[1].fX, q[2].fX),
              SkScalarAve(q[1].fY, q[2].fY) }
        };
        SkPoint s = { SkScalarAve(r[0].fX, r[1].fX), SkScalarAve(r[0].fY, r[1].fY) };
        innerPointsLeft >>= 1;
        inner[0] = innerPts[0];
        inner[1] = q[0];
        inner[2] = r[0];
        inner[3] = s;
        outer[0] = outer[1] = outer[2] = outer[3] = outerPts[0];
        a = generateShapedCubicPoints(outer, inner, tolSqd, points,
                                      1, innerPointsLeft);
        inner[0] = s;
        inner[1] = r[1];
        inner[2] = q[2];
        inner[3] = innerPts[3];
        outer[0] = outer[1] = outer[2] = outer[3] = outerPts[3];
        b = generateShapedCubicPoints(outer, inner, tolSqd, points,
                                      1, innerPointsLeft);
        (*points)[0] = outerPts[0];
        (*points)[1] = s;
        (*points)[2] = outerPts[3];
        *points += 3;
    }
    else {
        SkPoint outerQ[] = {
            { SkScalarAve(outerPts[0].fX, outerPts[1].fX),
              SkScalarAve(outerPts[0].fY, outerPts[1].fY) },
            { SkScalarAve(outerPts[1].fX, outerPts[2].fX),
              SkScalarAve(outerPts[1].fY, outerPts[2].fY) },
            { SkScalarAve(outerPts[2].fX, outerPts[3].fX),
              SkScalarAve(outerPts[2].fY, outerPts[3].fY) }
        };
        SkPoint outerR[] = {
            { SkScalarAve(outerQ[0].fX, outerQ[1].fX),
              SkScalarAve(outerQ[0].fY, outerQ[1].fY) },
            { SkScalarAve(outerQ[1].fX, outerQ[2].fX),
              SkScalarAve(outerQ[1].fY, outerQ[2].fY) }
        };
        SkPoint outerS = { SkScalarAve(outerR[0].fX, outerR[1].fX), SkScalarAve(outerR[0].fY, outerR[1].fY) };

        SkPoint innerQ[] = {
            { SkScalarAve(innerPts[0].fX, innerPts[1].fX),
              SkScalarAve(innerPts[0].fY, innerPts[1].fY) },
            { SkScalarAve(innerPts[1].fX, innerPts[2].fX),
              SkScalarAve(innerPts[1].fY, innerPts[2].fY) },
            { SkScalarAve(innerPts[2].fX, innerPts[3].fX),
              SkScalarAve(innerPts[2].fY, innerPts[3].fY) }
        };
        SkPoint innerR[] = {
            { SkScalarAve(innerQ[0].fX, innerQ[1].fX),
              SkScalarAve(innerQ[0].fY, innerQ[1].fY) },
            { SkScalarAve(innerQ[1].fX, innerQ[2].fX),
              SkScalarAve(innerQ[1].fY, innerQ[2].fY) }
        };
        SkPoint innerS = { SkScalarAve(innerR[0].fX, innerR[1].fX), SkScalarAve(innerR[0].fY, innerR[1].fY) };

        innerPointsLeft >>= 1;
        inner[0] = innerPts[0];
        inner[1] = innerQ[0];
        inner[2] = innerR[0];
        inner[3] = innerS;

        outerPointsLeft >>= 1;
        outer[0] = outerPts[0];
        outer[1] = outerQ[0];
        outer[2] = outerR[0];
        outer[3] = outerS;

        a = generateShapedCubicPoints(outer, inner, tolSqd, points,
                                      outerPointsLeft, innerPointsLeft);
        inner[0] = innerS;
        inner[1] = innerR[1];
        inner[2] = innerQ[2];
        inner[3] = innerPts[3];

        outer[0] = outerS;
        outer[1] = outerR[1];
        outer[2] = outerQ[2];
        outer[3] = outerPts[3];

        b = generateShapedCubicPoints(outer, inner, tolSqd, points,
                                outerPointsLeft, innerPointsLeft);
    }

    return a + b;
}

int GrPathUtils::worstCasePointCount(const SkPath& path, int* subpaths,
                                     SkScalar tol) {
    if (tol < gMinCurveTol) {
        tol = gMinCurveTol;
    }
    SkASSERT(tol > 0);

    int pointCount = 0;
    *subpaths = 1;

    bool first = true;

    SkPath::Iter iter(path, false);
    SkPath::Verb verb;

    SkPoint pts[4];
    while ((verb = iter.next(pts)) != SkPath::kDone_Verb) {

        switch (verb) {
            case SkPath::kLine_Verb:
                pointCount += 1;
                break;
            case SkPath::kQuad_Verb:
                pointCount += quadraticPointCount(pts, tol);
                break;
            case SkPath::kCubic_Verb:
                pointCount += cubicPointCount(pts, tol);
                break;
            case SkPath::kMove_Verb:
                pointCount += 1;
                if (!first) {
                    ++(*subpaths);
                }
                break;
            default:
                break;
        }
        first = false;
    }
    return pointCount;
}

void GrPathUtils::QuadUVMatrix::set(const SkPoint qPts[3]) {
    SkMatrix m;
    // We want M such that M * xy_pt = uv_pt
    // We know M * control_pts = [0  1/2 1]
    //                           [0  0   1]
    //                           [1  1   1]
    // And control_pts = [x0 x1 x2]
    //                   [y0 y1 y2]
    //                   [1  1  1 ]
    // We invert the control pt matrix and post concat to both sides to get M.
    // Using the known form of the control point matrix and the result, we can
    // optimize and improve precision.

    double x0 = qPts[0].fX;
    double y0 = qPts[0].fY;
    double x1 = qPts[1].fX;
    double y1 = qPts[1].fY;
    double x2 = qPts[2].fX;
    double y2 = qPts[2].fY;
    double det = x0*y1 - y0*x1 + x2*y0 - y2*x0 + x1*y2 - y1*x2;

    if (!sk_float_isfinite(det)
        || SkScalarNearlyZero((float)det, SK_ScalarNearlyZero * SK_ScalarNearlyZero)) {
        // The quad is degenerate. Hopefully this is rare. Find the pts that are
        // farthest apart to compute a line (unless it is really a pt).
        SkScalar maxD = qPts[0].distanceToSqd(qPts[1]);
        int maxEdge = 0;
        SkScalar d = qPts[1].distanceToSqd(qPts[2]);
        if (d > maxD) {
            maxD = d;
            maxEdge = 1;
        }
        d = qPts[2].distanceToSqd(qPts[0]);
        if (d > maxD) {
            maxD = d;
            maxEdge = 2;
        }
        // We could have a tolerance here, not sure if it would improve anything
        if (maxD > 0) {
            // Set the matrix to give (u = 0, v = distance_to_line)
            SkVector lineVec = qPts[(maxEdge + 1)%3] - qPts[maxEdge];
            // when looking from the point 0 down the line we want positive
            // distances to be to the left. This matches the non-degenerate
            // case.
            lineVec.setOrthog(lineVec, SkPoint::kLeft_Side);
            lineVec.dot(qPts[0]);
            // first row
            fM[0] = 0;
            fM[1] = 0;
            fM[2] = 0;
            // second row
            fM[3] = lineVec.fX;
            fM[4] = lineVec.fY;
            fM[5] = -lineVec.dot(qPts[maxEdge]);
        } else {
            // It's a point. It should cover zero area. Just set the matrix such
            // that (u, v) will always be far away from the quad.
            fM[0] = 0; fM[1] = 0; fM[2] = 100.f;
            fM[3] = 0; fM[4] = 0; fM[5] = 100.f;
        }
    } else {
        double scale = 1.0/det;

        // compute adjugate matrix
        double a0, a1, a2, a3, a4, a5, a6, a7, a8;
        a0 = y1-y2;
        a1 = x2-x1;
        a2 = x1*y2-x2*y1;

        a3 = y2-y0;
        a4 = x0-x2;
        a5 = x2*y0-x0*y2;

        a6 = y0-y1;
        a7 = x1-x0;
        a8 = x0*y1-x1*y0;

        // this performs the uv_pts*adjugate(control_pts) multiply,
        // then does the scale by 1/det afterwards to improve precision
        m[SkMatrix::kMScaleX] = (float)((0.5*a3 + a6)*scale);
        m[SkMatrix::kMSkewX]  = (float)((0.5*a4 + a7)*scale);
        m[SkMatrix::kMTransX] = (float)((0.5*a5 + a8)*scale);

        m[SkMatrix::kMSkewY]  = (float)(a6*scale);
        m[SkMatrix::kMScaleY] = (float)(a7*scale);
        m[SkMatrix::kMTransY] = (float)(a8*scale);

        m[SkMatrix::kMPersp0] = (float)((a0 + a3 + a6)*scale);
        m[SkMatrix::kMPersp1] = (float)((a1 + a4 + a7)*scale);
        m[SkMatrix::kMPersp2] = (float)((a2 + a5 + a8)*scale);

        // The matrix should not have perspective.
        SkDEBUGCODE(static const SkScalar gTOL = 1.f / 100.f);
        SkASSERT(SkScalarAbs(m.get(SkMatrix::kMPersp0)) < gTOL);
        SkASSERT(SkScalarAbs(m.get(SkMatrix::kMPersp1)) < gTOL);

        // It may not be normalized to have 1.0 in the bottom right
        float m33 = m.get(SkMatrix::kMPersp2);
        if (1.f != m33) {
            m33 = 1.f / m33;
            fM[0] = m33 * m.get(SkMatrix::kMScaleX);
            fM[1] = m33 * m.get(SkMatrix::kMSkewX);
            fM[2] = m33 * m.get(SkMatrix::kMTransX);
            fM[3] = m33 * m.get(SkMatrix::kMSkewY);
            fM[4] = m33 * m.get(SkMatrix::kMScaleY);
            fM[5] = m33 * m.get(SkMatrix::kMTransY);
        } else {
            fM[0] = m.get(SkMatrix::kMScaleX);
            fM[1] = m.get(SkMatrix::kMSkewX);
            fM[2] = m.get(SkMatrix::kMTransX);
            fM[3] = m.get(SkMatrix::kMSkewY);
            fM[4] = m.get(SkMatrix::kMScaleY);
            fM[5] = m.get(SkMatrix::kMTransY);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

// k = (y2 - y0, x0 - x2, (x2 - x0)*y0 - (y2 - y0)*x0 )
// l = (2*w * (y1 - y0), 2*w * (x0 - x1), 2*w * (x1*y0 - x0*y1))
// m = (2*w * (y2 - y1), 2*w * (x1 - x2), 2*w * (x2*y1 - x1*y2))
void GrPathUtils::getConicKLM(const SkPoint p[3], const SkScalar weight, SkScalar klm[9]) {
    const SkScalar w2 = 2.f * weight;
    klm[0] = p[2].fY - p[0].fY;
    klm[1] = p[0].fX - p[2].fX;
    klm[2] = (p[2].fX - p[0].fX) * p[0].fY - (p[2].fY - p[0].fY) * p[0].fX;

    klm[3] = w2 * (p[1].fY - p[0].fY);
    klm[4] = w2 * (p[0].fX - p[1].fX);
    klm[5] = w2 * (p[1].fX * p[0].fY - p[0].fX * p[1].fY);

    klm[6] = w2 * (p[2].fY - p[1].fY);
    klm[7] = w2 * (p[1].fX - p[2].fX);
    klm[8] = w2 * (p[2].fX * p[1].fY - p[1].fX * p[2].fY);

    // scale the max absolute value of coeffs to 10
    SkScalar scale = 0.f;
    for (int i = 0; i < 9; ++i) {
       scale = SkMaxScalar(scale, SkScalarAbs(klm[i]));
    }
    SkASSERT(scale > 0.f);
    scale = 10.f / scale;
    for (int i = 0; i < 9; ++i) {
        klm[i] *= scale;
    }
}

////////////////////////////////////////////////////////////////////////////////

namespace {

// a is the first control point of the cubic.
// ab is the vector from a to the second control point.
// dc is the vector from the fourth to the third control point.
// d is the fourth control point.
// p is the candidate quadratic control point.
// this assumes that the cubic doesn't inflect and is simple
bool is_point_within_cubic_tangents(const SkPoint& a,
                                    const SkVector& ab,
                                    const SkVector& dc,
                                    const SkPoint& d,
                                    SkPath::Direction dir,
                                    const SkPoint p) {
    SkVector ap = p - a;
    SkScalar apXab = ap.cross(ab);
    if (SkPath::kCW_Direction == dir) {
        if (apXab > 0) {
            return false;
        }
    } else {
        SkASSERT(SkPath::kCCW_Direction == dir);
        if (apXab < 0) {
            return false;
        }
    }

    SkVector dp = p - d;
    SkScalar dpXdc = dp.cross(dc);
    if (SkPath::kCW_Direction == dir) {
        if (dpXdc < 0) {
            return false;
        }
    } else {
        SkASSERT(SkPath::kCCW_Direction == dir);
        if (dpXdc > 0) {
            return false;
        }
    }
    return true;
}

void convert_noninflect_cubic_to_quads(const SkPoint p[4],
                                       SkScalar toleranceSqd,
                                       bool constrainWithinTangents,
                                       SkPath::Direction dir,
                                       SkTArray<SkPoint, true>* quads,
                                       int sublevel = 0) {

    // Notation: Point a is always p[0]. Point b is p[1] unless p[1] == p[0], in which case it is
    // p[2]. Point d is always p[3]. Point c is p[2] unless p[2] == p[3], in which case it is p[1].

    SkVector ab = p[1] - p[0];
    SkVector dc = p[2] - p[3];

    if (ab.isZero()) {
        if (dc.isZero()) {
            SkPoint* degQuad = quads->push_back_n(3);
            degQuad[0] = p[0];
            degQuad[1] = p[0];
            degQuad[2] = p[3];
            return;
        }
        ab = p[2] - p[0];
    }
    if (dc.isZero()) {
        dc = p[1] - p[3];
    }

    // When the ab and cd tangents are nearly parallel with vector from d to a the constraint that
    // the quad point falls between the tangents becomes hard to enforce and we are likely to hit
    // the max subdivision count. However, in this case the cubic is approaching a line and the
    // accuracy of the quad point isn't so important. We check if the two middle cubic control
    // points are very close to the baseline vector. If so then we just pick quadratic points on the
    // control polygon.

    if (constrainWithinTangents) {
        SkVector da = p[0] - p[3];
        SkScalar invDALengthSqd = da.lengthSqd();
        if (invDALengthSqd > SK_ScalarNearlyZero) {
            invDALengthSqd = SkScalarInvert(invDALengthSqd);
            // cross(ab, da)^2/length(da)^2 == sqd distance from b to line from d to a.
            // same goed for point c using vector cd.
            SkScalar detABSqd = ab.cross(da);
            detABSqd = SkScalarSquare(detABSqd);
            SkScalar detDCSqd = dc.cross(da);
            detDCSqd = SkScalarSquare(detDCSqd);
            if (SkScalarMul(detABSqd, invDALengthSqd) < toleranceSqd &&
                SkScalarMul(detDCSqd, invDALengthSqd) < toleranceSqd) {
                SkPoint b = p[0] + ab;
                SkPoint c = p[3] + dc;
                SkPoint mid = b + c;
                mid.scale(SK_ScalarHalf);
                // Insert two quadratics to cover the case when ab points away from d and/or dc
                // points away from a.
                if (SkVector::DotProduct(da, dc) < 0 || SkVector::DotProduct(ab,da) > 0) {
                    SkPoint* qpts = quads->push_back_n(6);
                    qpts[0] = p[0];
                    qpts[1] = b;
                    qpts[2] = mid;
                    qpts[3] = mid;
                    qpts[4] = c;
                    qpts[5] = p[3];
                } else {
                    SkPoint* qpts = quads->push_back_n(3);
                    qpts[0] = p[0];
                    qpts[1] = mid;
                    qpts[2] = p[3];
                }
                return;
            }
        }
    }

    static const SkScalar kLengthScale = 3 * SK_Scalar1 / 2;
    static const int kMaxSubdivs = 10;

    ab.scale(kLengthScale);
    dc.scale(kLengthScale);

    // e0 and e1 are extrapolations along vectors ab and dc.
    SkVector c0 = p[0];
    c0 += ab;
    SkVector c1 = p[3];
    c1 += dc;

    SkScalar dSqd = sublevel > kMaxSubdivs ? 0 : c0.distanceToSqd(c1);
    if (dSqd < toleranceSqd) {
        SkPoint cAvg = c0;
        cAvg += c1;
        cAvg.scale(SK_ScalarHalf);

        bool subdivide = false;

        if (constrainWithinTangents &&
            !is_point_within_cubic_tangents(p[0], ab, dc, p[3], dir, cAvg)) {
            // choose a new cAvg that is the intersection of the two tangent lines.
            ab.setOrthog(ab);
            SkScalar z0 = -ab.dot(p[0]);
            dc.setOrthog(dc);
            SkScalar z1 = -dc.dot(p[3]);
            cAvg.fX = SkScalarMul(ab.fY, z1) - SkScalarMul(z0, dc.fY);
            cAvg.fY = SkScalarMul(z0, dc.fX) - SkScalarMul(ab.fX, z1);
            SkScalar z = SkScalarMul(ab.fX, dc.fY) - SkScalarMul(ab.fY, dc.fX);
            z = SkScalarInvert(z);
            cAvg.fX *= z;
            cAvg.fY *= z;
            if (sublevel <= kMaxSubdivs) {
                SkScalar d0Sqd = c0.distanceToSqd(cAvg);
                SkScalar d1Sqd = c1.distanceToSqd(cAvg);
                // We need to subdivide if d0 + d1 > tolerance but we have the sqd values. We know
                // the distances and tolerance can't be negative.
                // (d0 + d1)^2 > toleranceSqd
                // d0Sqd + 2*d0*d1 + d1Sqd > toleranceSqd
                SkScalar d0d1 = SkScalarSqrt(SkScalarMul(d0Sqd, d1Sqd));
                subdivide = 2 * d0d1 + d0Sqd + d1Sqd > toleranceSqd;
            }
        }
        if (!subdivide) {
            SkPoint* pts = quads->push_back_n(3);
            pts[0] = p[0];
            pts[1] = cAvg;
            pts[2] = p[3];
            return;
        }
    }
    SkPoint choppedPts[7];
    SkChopCubicAtHalf(p, choppedPts);
    convert_noninflect_cubic_to_quads(choppedPts + 0,
                                      toleranceSqd,
                                      constrainWithinTangents,
                                      dir,
                                      quads,
                                      sublevel + 1);
    convert_noninflect_cubic_to_quads(choppedPts + 3,
                                      toleranceSqd,
                                      constrainWithinTangents,
                                      dir,
                                      quads,
                                      sublevel + 1);
}
}

void GrPathUtils::convertCubicToQuads(const SkPoint p[4],
                                      SkScalar tolScale,
                                      bool constrainWithinTangents,
                                      SkPath::Direction dir,
                                      SkTArray<SkPoint, true>* quads) {
    SkPoint chopped[10];
    int count = SkChopCubicAtInflections(p, chopped);

    // base tolerance is 1 pixel.
    static const SkScalar kTolerance = SK_Scalar1;
    const SkScalar tolSqd = SkScalarSquare(SkScalarMul(tolScale, kTolerance));

    for (int i = 0; i < count; ++i) {
        SkPoint* cubic = chopped + 3*i;
        convert_noninflect_cubic_to_quads(cubic, tolSqd, constrainWithinTangents, dir, quads);
    }

}

////////////////////////////////////////////////////////////////////////////////

enum CubicType {
    kSerpentine_CubicType,
    kCusp_CubicType,
    kLoop_CubicType,
    kQuadratic_CubicType,
    kLine_CubicType,
    kPoint_CubicType
};

// discr(I) = d0^2 * (3*d1^2 - 4*d0*d2)
// Classification:
// discr(I) > 0        Serpentine
// discr(I) = 0        Cusp
// discr(I) < 0        Loop
// d0 = d1 = 0         Quadratic
// d0 = d1 = d2 = 0    Line
// p0 = p1 = p2 = p3   Point
static CubicType classify_cubic(const SkPoint p[4], const SkScalar d[3]) {
    if (p[0] == p[1] && p[0] == p[2] && p[0] == p[3]) {
        return kPoint_CubicType;
    }
    const SkScalar discr = d[0] * d[0] * (3.f * d[1] * d[1] - 4.f * d[0] * d[2]);
    if (discr > SK_ScalarNearlyZero) {
        return kSerpentine_CubicType;
    } else if (discr < -SK_ScalarNearlyZero) {
        return kLoop_CubicType;
    } else {
        if (0.f == d[0] && 0.f == d[1]) {
            return (0.f == d[2] ? kLine_CubicType : kQuadratic_CubicType);
        } else {
            return kCusp_CubicType;
        }
    }
}

// Assumes the third component of points is 1.
// Calcs p0 . (p1 x p2)
static SkScalar calc_dot_cross_cubic(const SkPoint& p0, const SkPoint& p1, const SkPoint& p2) {
    const SkScalar xComp = p0.fX * (p1.fY - p2.fY);
    const SkScalar yComp = p0.fY * (p2.fX - p1.fX);
    const SkScalar wComp = p1.fX * p2.fY - p1.fY * p2.fX;
    return (xComp + yComp + wComp);
}

// Solves linear system to extract klm
// P.K = k (similarly for l, m)
// Where P is matrix of control points
// K is coefficients for the line K
// k is vector of values of K evaluated at the control points
// Solving for K, thus K = P^(-1) . k
static void calc_cubic_klm(const SkPoint p[4], const SkScalar controlK[4],
                           const SkScalar controlL[4], const SkScalar controlM[4],
                           SkScalar k[3], SkScalar l[3], SkScalar m[3]) {
    SkMatrix matrix;
    matrix.setAll(p[0].fX, p[0].fY, 1.f,
                  p[1].fX, p[1].fY, 1.f,
                  p[2].fX, p[2].fY, 1.f);
    SkMatrix inverse;
    if (matrix.invert(&inverse)) {
       inverse.mapHomogeneousPoints(k, controlK, 1);
       inverse.mapHomogeneousPoints(l, controlL, 1);
       inverse.mapHomogeneousPoints(m, controlM, 1);
    }

}

static void set_serp_klm(const SkScalar d[3], SkScalar k[4], SkScalar l[4], SkScalar m[4]) {
    SkScalar tempSqrt = SkScalarSqrt(9.f * d[1] * d[1] - 12.f * d[0] * d[2]);
    SkScalar ls = 3.f * d[1] - tempSqrt;
    SkScalar lt = 6.f * d[0];
    SkScalar ms = 3.f * d[1] + tempSqrt;
    SkScalar mt = 6.f * d[0];

    k[0] = ls * ms;
    k[1] = (3.f * ls * ms - ls * mt - lt * ms) / 3.f;
    k[2] = (lt * (mt - 2.f * ms) + ls * (3.f * ms - 2.f * mt)) / 3.f;
    k[3] = (lt - ls) * (mt - ms);

    l[0] = ls * ls * ls;
    const SkScalar lt_ls = lt - ls;
    l[1] = ls * ls * lt_ls * -1.f;
    l[2] = lt_ls * lt_ls * ls;
    l[3] = -1.f * lt_ls * lt_ls * lt_ls;

    m[0] = ms * ms * ms;
    const SkScalar mt_ms = mt - ms;
    m[1] = ms * ms * mt_ms * -1.f;
    m[2] = mt_ms * mt_ms * ms;
    m[3] = -1.f * mt_ms * mt_ms * mt_ms;

    // If d0 < 0 we need to flip the orientation of our curve
    // This is done by negating the k and l values
    // We want negative distance values to be on the inside
    if ( d[0] > 0) {
        for (int i = 0; i < 4; ++i) {
            k[i] = -k[i];
            l[i] = -l[i];
        }
    }
}

static void set_loop_klm(const SkScalar d[3], SkScalar k[4], SkScalar l[4], SkScalar m[4]) {
    SkScalar tempSqrt = SkScalarSqrt(4.f * d[0] * d[2] - 3.f * d[1] * d[1]);
    SkScalar ls = d[1] - tempSqrt;
    SkScalar lt = 2.f * d[0];
    SkScalar ms = d[1] + tempSqrt;
    SkScalar mt = 2.f * d[0];

    k[0] = ls * ms;
    k[1] = (3.f * ls*ms - ls * mt - lt * ms) / 3.f;
    k[2] = (lt * (mt - 2.f * ms) + ls * (3.f * ms - 2.f * mt)) / 3.f;
    k[3] = (lt - ls) * (mt - ms);

    l[0] = ls * ls * ms;
    l[1] = (ls * (ls * (mt - 3.f * ms) + 2.f * lt * ms))/-3.f;
    l[2] = ((lt - ls) * (ls * (2.f * mt - 3.f * ms) + lt * ms))/3.f;
    l[3] = -1.f * (lt - ls) * (lt - ls) * (mt - ms);

    m[0] = ls * ms * ms;
    m[1] = (ms * (ls * (2.f * mt - 3.f * ms) + lt * ms))/-3.f;
    m[2] = ((mt - ms) * (ls * (mt - 3.f * ms) + 2.f * lt * ms))/3.f;
    m[3] = -1.f * (lt - ls) * (mt - ms) * (mt - ms);


    // If (d0 < 0 && sign(k1) > 0) || (d0 > 0 && sign(k1) < 0),
    // we need to flip the orientation of our curve.
    // This is done by negating the k and l values
    if ( (d[0] < 0 && k[1] > 0) || (d[0] > 0 && k[1] < 0)) {
        for (int i = 0; i < 4; ++i) {
            k[i] = -k[i];
            l[i] = -l[i];
        }
    }
}

static void set_cusp_klm(const SkScalar d[3], SkScalar k[4], SkScalar l[4], SkScalar m[4]) {
    const SkScalar ls = d[2];
    const SkScalar lt = 3.f * d[1];

    k[0] = ls;
    k[1] = ls - lt / 3.f;
    k[2] = ls - 2.f * lt / 3.f;
    k[3] = ls - lt;

    l[0] = ls * ls * ls;
    const SkScalar ls_lt = ls - lt;
    l[1] = ls * ls * ls_lt;
    l[2] = ls_lt * ls_lt * ls;
    l[3] = ls_lt * ls_lt * ls_lt;

    m[0] = 1.f;
    m[1] = 1.f;
    m[2] = 1.f;
    m[3] = 1.f;
}

// For the case when a cubic is actually a quadratic
// M =
// 0     0     0
// 1/3   0     1/3
// 2/3   1/3   2/3
// 1     1     1
static void set_quadratic_klm(const SkScalar d[3], SkScalar k[4], SkScalar l[4], SkScalar m[4]) {
    k[0] = 0.f;
    k[1] = 1.f/3.f;
    k[2] = 2.f/3.f;
    k[3] = 1.f;

    l[0] = 0.f;
    l[1] = 0.f;
    l[2] = 1.f/3.f;
    l[3] = 1.f;

    m[0] = 0.f;
    m[1] = 1.f/3.f;
    m[2] = 2.f/3.f;
    m[3] = 1.f;

    // If d2 < 0 we need to flip the orientation of our curve
    // This is done by negating the k and l values
    if ( d[2] > 0) {
        for (int i = 0; i < 4; ++i) {
            k[i] = -k[i];
            l[i] = -l[i];
        }
    }
}

// Calc coefficients of I(s,t) where roots of I are inflection points of curve
// I(s,t) = t*(3*d0*s^2 - 3*d1*s*t + d2*t^2)
// d0 = a1 - 2*a2+3*a3
// d1 = -a2 + 3*a3
// d2 = 3*a3
// a1 = p0 . (p3 x p2)
// a2 = p1 . (p0 x p3)
// a3 = p2 . (p1 x p0)
// Places the values of d1, d2, d3 in array d passed in
static void calc_cubic_inflection_func(const SkPoint p[4], SkScalar d[3]) {
    SkScalar a1 = calc_dot_cross_cubic(p[0], p[3], p[2]);
    SkScalar a2 = calc_dot_cross_cubic(p[1], p[0], p[3]);
    SkScalar a3 = calc_dot_cross_cubic(p[2], p[1], p[0]);

    // need to scale a's or values in later calculations will grow to high
    SkScalar max = SkScalarAbs(a1);
    max = SkMaxScalar(max, SkScalarAbs(a2));
    max = SkMaxScalar(max, SkScalarAbs(a3));
    max = 1.f/max;
    a1 = a1 * max;
    a2 = a2 * max;
    a3 = a3 * max;

    d[2] = 3.f * a3;
    d[1] = d[2] - a2;
    d[0] = d[1] - a2 + a1;
}

int GrPathUtils::chopCubicAtLoopIntersection(const SkPoint src[4], SkPoint dst[10], SkScalar klm[9],
                                             SkScalar klm_rev[3]) {
    // Variable to store the two parametric values at the loop double point
    SkScalar smallS = 0.f;
    SkScalar largeS = 0.f;

    SkScalar d[3];
    calc_cubic_inflection_func(src, d);

    CubicType cType = classify_cubic(src, d);

    int chop_count = 0;
    if (kLoop_CubicType == cType) {
        SkScalar tempSqrt = SkScalarSqrt(4.f * d[0] * d[2] - 3.f * d[1] * d[1]);
        SkScalar ls = d[1] - tempSqrt;
        SkScalar lt = 2.f * d[0];
        SkScalar ms = d[1] + tempSqrt;
        SkScalar mt = 2.f * d[0];
        ls = ls / lt;
        ms = ms / mt;
        // need to have t values sorted since this is what is expected by SkChopCubicAt
        if (ls <= ms) {
            smallS = ls;
            largeS = ms;
        } else {
            smallS = ms;
            largeS = ls;
        }

        SkScalar chop_ts[2];
        if (smallS > 0.f && smallS < 1.f) {
            chop_ts[chop_count++] = smallS;
        }
        if (largeS > 0.f && largeS < 1.f) {
            chop_ts[chop_count++] = largeS;
        }
        if(dst) {
            SkChopCubicAt(src, dst, chop_ts, chop_count);
        }
    } else {
        if (dst) {
            memcpy(dst, src, sizeof(SkPoint) * 4);
        }
    }

    if (klm && klm_rev) {
        // Set klm_rev to to match the sub_section of cubic that needs to have its orientation
        // flipped. This will always be the section that is the "loop"
        if (2 == chop_count) {
            klm_rev[0] = 1.f;
            klm_rev[1] = -1.f;
            klm_rev[2] = 1.f;
        } else if (1 == chop_count) {
            if (smallS < 0.f) {
                klm_rev[0] = -1.f;
                klm_rev[1] = 1.f;
            } else {
                klm_rev[0] = 1.f;
                klm_rev[1] = -1.f;
            }
        } else {
            if (smallS < 0.f && largeS > 1.f) {
                klm_rev[0] = -1.f;
            } else {
                klm_rev[0] = 1.f;
            }
        }
        SkScalar controlK[4];
        SkScalar controlL[4];
        SkScalar controlM[4];

        if (kSerpentine_CubicType == cType || (kCusp_CubicType == cType && 0.f != d[0])) {
            set_serp_klm(d, controlK, controlL, controlM);
        } else if (kLoop_CubicType == cType) {
            set_loop_klm(d, controlK, controlL, controlM);
        } else if (kCusp_CubicType == cType) {
            SkASSERT(0.f == d[0]);
            set_cusp_klm(d, controlK, controlL, controlM);
        } else if (kQuadratic_CubicType == cType) {
            set_quadratic_klm(d, controlK, controlL, controlM);
        }

        calc_cubic_klm(src, controlK, controlL, controlM, klm, &klm[3], &klm[6]);
    }
    return chop_count + 1;
}

void GrPathUtils::getCubicKLM(const SkPoint p[4], SkScalar klm[9]) {
    SkScalar d[3];
    calc_cubic_inflection_func(p, d);

    CubicType cType = classify_cubic(p, d);

    SkScalar controlK[4];
    SkScalar controlL[4];
    SkScalar controlM[4];

    if (kSerpentine_CubicType == cType || (kCusp_CubicType == cType && 0.f != d[0])) {
        set_serp_klm(d, controlK, controlL, controlM);
    } else if (kLoop_CubicType == cType) {
        set_loop_klm(d, controlK, controlL, controlM);
    } else if (kCusp_CubicType == cType) {
        SkASSERT(0.f == d[0]);
        set_cusp_klm(d, controlK, controlL, controlM);
    } else if (kQuadratic_CubicType == cType) {
        set_quadratic_klm(d, controlK, controlL, controlM);
    }

    calc_cubic_klm(p, controlK, controlL, controlM, klm, &klm[3], &klm[6]);
}
