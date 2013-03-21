/*
 * Copyright (C) 2011-2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef RS_SERVER
#include <cutils/compiler.h>
#endif

#include "rsContext.h"
#include "rsScriptC.h"
#include "rsMatrix4x4.h"
#include "rsMatrix3x3.h"
#include "rsMatrix2x2.h"

#include "rsCpuCore.h"
#include "rsCpuScript.h"


using namespace android;
using namespace android::renderscript;


//////////////////////////////////////////////////////////////////////////////
// Float util
//////////////////////////////////////////////////////////////////////////////


static void SC_MatrixLoadRotate(Matrix4x4 *m, float rot, float x, float y, float z) {
    m->loadRotate(rot, x, y, z);
}
static void SC_MatrixLoadScale(Matrix4x4 *m, float x, float y, float z) {
    m->loadScale(x, y, z);
}
static void SC_MatrixLoadTranslate(Matrix4x4 *m, float x, float y, float z) {
    m->loadTranslate(x, y, z);
}
static void SC_MatrixRotate(Matrix4x4 *m, float rot, float x, float y, float z) {
    m->rotate(rot, x, y, z);
}
static void SC_MatrixScale(Matrix4x4 *m, float x, float y, float z) {
    m->scale(x, y, z);
}
static void SC_MatrixTranslate(Matrix4x4 *m, float x, float y, float z) {
    m->translate(x, y, z);
}

static void SC_MatrixLoadOrtho(Matrix4x4 *m, float l, float r, float b, float t, float n, float f) {
    m->loadOrtho(l, r, b, t, n, f);
}
static void SC_MatrixLoadFrustum(Matrix4x4 *m, float l, float r, float b, float t, float n, float f) {
    m->loadFrustum(l, r, b, t, n, f);
}
static void SC_MatrixLoadPerspective(Matrix4x4 *m, float fovy, float aspect, float near, float far) {
    m->loadPerspective(fovy, aspect, near, far);
}

static bool SC_MatrixInverse_4x4(Matrix4x4 *m) {
    return m->inverse();
}
static bool SC_MatrixInverseTranspose_4x4(Matrix4x4 *m) {
    return m->inverseTranspose();
}
static void SC_MatrixTranspose_4x4(Matrix4x4 *m) {
    m->transpose();
}
static void SC_MatrixTranspose_3x3(Matrix3x3 *m) {
    m->transpose();
}
static void SC_MatrixTranspose_2x2(Matrix2x2 *m) {
    m->transpose();
}

static float SC_randf2(float min, float max) {
    float r = (float)rand();
    r /= RAND_MAX;
    r = r * (max - min) + min;
    return r;
}

static float SC_frac(float v) {
    int i = (int)floor(v);
    return fmin(v - i, 0x1.fffffep-1f);
}


//////////////////////////////////////////////////////////////////////////////
// Class implementation
//////////////////////////////////////////////////////////////////////////////

// llvm name mangling ref
//  <builtin-type> ::= v  # void
//                 ::= b  # bool
//                 ::= c  # char
//                 ::= a  # signed char
//                 ::= h  # unsigned char
//                 ::= s  # short
//                 ::= t  # unsigned short
//                 ::= i  # int
//                 ::= j  # unsigned int
//                 ::= l  # long
//                 ::= m  # unsigned long
//                 ::= x  # long long, __int64
//                 ::= y  # unsigned long long, __int64
//                 ::= f  # float
//                 ::= d  # double

static RsdCpuReference::CpuSymbol gSyms[] = {
    { "_Z4acosf", (void *)&acosf, true },
    { "_Z5acoshf", (void *)&acoshf, true },
    { "_Z4asinf", (void *)&asinf, true },
    { "_Z5asinhf", (void *)&asinhf, true },
    { "_Z4atanf", (void *)&atanf, true },
    { "_Z5atan2ff", (void *)&atan2f, true },
    { "_Z5atanhf", (void *)&atanhf, true },
    { "_Z4cbrtf", (void *)&cbrtf, true },
    { "_Z4ceilf", (void *)&ceilf, true },
    { "_Z8copysignff", (void *)&copysignf, true },
    { "_Z3cosf", (void *)&cosf, true },
    { "_Z4coshf", (void *)&coshf, true },
    { "_Z4erfcf", (void *)&erfcf, true },
    { "_Z3erff", (void *)&erff, true },
    { "_Z3expf", (void *)&expf, true },
    { "_Z4exp2f", (void *)&exp2f, true },
    { "_Z5expm1f", (void *)&expm1f, true },
    { "_Z4fdimff", (void *)&fdimf, true },
    { "_Z5floorf", (void *)&floorf, true },
    { "_Z3fmafff", (void *)&fmaf, true },
    { "_Z4fmaxff", (void *)&fmaxf, true },
    { "_Z4fminff", (void *)&fminf, true },  // float fmin(float, float)
    { "_Z4fmodff", (void *)&fmodf, true },
    { "_Z5frexpfPi", (void *)&frexpf, true },
    { "_Z5hypotff", (void *)&hypotf, true },
    { "_Z5ilogbf", (void *)&ilogbf, true },
    { "_Z5ldexpfi", (void *)&ldexpf, true },
    { "_Z6lgammaf", (void *)&lgammaf, true },
    { "_Z6lgammafPi", (void *)&lgammaf_r, true },
    { "_Z3logf", (void *)&logf, true },
    { "_Z5log10f", (void *)&log10f, true },
    { "_Z5log1pf", (void *)&log1pf, true },
    { "_Z4logbf", (void *)&logbf, true },
    { "_Z4modffPf", (void *)&modff, true },
    //{ "_Z3nanj", (void *)&SC_nan, true },
    { "_Z9nextafterff", (void *)&nextafterf, true },
    { "_Z3powff", (void *)&powf, true },
    { "_Z9remainderff", (void *)&remainderf, true },
    { "_Z6remquoffPi", (void *)&remquof, true },
    { "_Z4rintf", (void *)&rintf, true },
    { "_Z5roundf", (void *)&roundf, true },
    { "_Z3sinf", (void *)&sinf, true },
    { "_Z4sinhf", (void *)&sinhf, true },
    { "_Z4sqrtf", (void *)&sqrtf, true },
    { "_Z3tanf", (void *)&tanf, true },
    { "_Z4tanhf", (void *)&tanhf, true },
    { "_Z6tgammaf", (void *)&tgammaf, true },
    { "_Z5truncf", (void *)&truncf, true },

    //{ "smoothstep", (void *)&, true },

    // matrix
    { "_Z18rsMatrixLoadRotateP12rs_matrix4x4ffff", (void *)&SC_MatrixLoadRotate, true },
    { "_Z17rsMatrixLoadScaleP12rs_matrix4x4fff", (void *)&SC_MatrixLoadScale, true },
    { "_Z21rsMatrixLoadTranslateP12rs_matrix4x4fff", (void *)&SC_MatrixLoadTranslate, true },
    { "_Z14rsMatrixRotateP12rs_matrix4x4ffff", (void *)&SC_MatrixRotate, true },
    { "_Z13rsMatrixScaleP12rs_matrix4x4fff", (void *)&SC_MatrixScale, true },
    { "_Z17rsMatrixTranslateP12rs_matrix4x4fff", (void *)&SC_MatrixTranslate, true },

    { "_Z17rsMatrixLoadOrthoP12rs_matrix4x4ffffff", (void *)&SC_MatrixLoadOrtho, true },
    { "_Z19rsMatrixLoadFrustumP12rs_matrix4x4ffffff", (void *)&SC_MatrixLoadFrustum, true },
    { "_Z23rsMatrixLoadPerspectiveP12rs_matrix4x4ffff", (void *)&SC_MatrixLoadPerspective, true },

    { "_Z15rsMatrixInverseP12rs_matrix4x4", (void *)&SC_MatrixInverse_4x4, true },
    { "_Z24rsMatrixInverseTransposeP12rs_matrix4x4", (void *)&SC_MatrixInverseTranspose_4x4, true },
    { "_Z17rsMatrixTransposeP12rs_matrix4x4", (void *)&SC_MatrixTranspose_4x4, true },
    { "_Z17rsMatrixTransposeP12rs_matrix3x3", (void *)&SC_MatrixTranspose_3x3, true },
    { "_Z17rsMatrixTransposeP12rs_matrix2x2", (void *)&SC_MatrixTranspose_2x2, true },

    // RS Math
    { "_Z6rsRandff", (void *)&SC_randf2, true },
    { "_Z6rsFracf", (void *)&SC_frac, true },

    { NULL, NULL, false }
};

const RsdCpuReference::CpuSymbol * RsdCpuScriptImpl::lookupSymbolMath(const char *sym) {
    const RsdCpuReference::CpuSymbol *syms = gSyms;

    while (syms->fnPtr) {
        if (!strcmp(syms->name, sym)) {
            return syms;
        }
        syms++;
    }
    return NULL;
}
