/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkBitmap.h"
#include "SkData.h"
#include "SkDecodingImageGenerator.h"
#include "SkForceLinking.h"
#include "SkImageDecoder.h"
#include "SkOSFile.h"
#include "SkRandom.h"
#include "SkStream.h"
#include "Test.h"

__SK_FORCE_IMAGE_DECODER_LINKING;

/**
 * First, make sure that writing an 8-bit RGBA KTX file and then
 * reading it produces the same bitmap.
 */
DEF_TEST(KtxReadWrite, reporter) {

    // Random number generator with explicit seed for reproducibility
    SkRandom rand(0x1005cbad);

    SkBitmap bm8888;
    bm8888.setConfig(SkBitmap::kARGB_8888_Config, 128, 128);

    bool pixelsAllocated = bm8888.allocPixels();
    REPORTER_ASSERT(reporter, pixelsAllocated);

    uint8_t *pixels = reinterpret_cast<uint8_t*>(bm8888.getPixels());
    REPORTER_ASSERT(reporter, NULL != pixels);

    if (NULL == pixels) {
        return;
    }
    
    uint8_t *row = pixels;
    for (int y = 0; y < bm8888.height(); ++y) {        
        for (int x = 0; x < bm8888.width(); ++x) {
            uint8_t a = rand.nextRangeU(0, 255);
            uint8_t r = rand.nextRangeU(0, 255);
            uint8_t g = rand.nextRangeU(0, 255);
            uint8_t b = rand.nextRangeU(0, 255);

            SkPMColor &pixel = *(reinterpret_cast<SkPMColor*>(row + x*sizeof(SkPMColor)));
            pixel = SkPreMultiplyARGB(a, r, g, b);
        }
        row += bm8888.rowBytes();
    }
    REPORTER_ASSERT(reporter, !(bm8888.empty()));

    SkAutoDataUnref encodedData(SkImageEncoder::EncodeData(bm8888, SkImageEncoder::kKTX_Type, 0));
    REPORTER_ASSERT(reporter, NULL != encodedData);

    SkAutoTUnref<SkMemoryStream> stream(SkNEW_ARGS(SkMemoryStream, (encodedData)));
    REPORTER_ASSERT(reporter, NULL != stream);

    SkBitmap decodedBitmap;
    bool imageDecodeSuccess = SkImageDecoder::DecodeStream(stream, &decodedBitmap);
    REPORTER_ASSERT(reporter, imageDecodeSuccess);

    REPORTER_ASSERT(reporter, decodedBitmap.config() == bm8888.config());
    REPORTER_ASSERT(reporter, decodedBitmap.alphaType() == bm8888.alphaType());
    REPORTER_ASSERT(reporter, decodedBitmap.width() == bm8888.width());
    REPORTER_ASSERT(reporter, decodedBitmap.height() == bm8888.height());
    REPORTER_ASSERT(reporter, !(decodedBitmap.empty()));

    uint8_t *decodedPixels = reinterpret_cast<uint8_t*>(decodedBitmap.getPixels());
    REPORTER_ASSERT(reporter, NULL != decodedPixels);
    REPORTER_ASSERT(reporter, decodedBitmap.getSize() == bm8888.getSize());

    if (NULL == decodedPixels) {
        return;
    }

    REPORTER_ASSERT(reporter, memcmp(decodedPixels, pixels, decodedBitmap.getSize()) == 0);
}

/**
 * Next test is to see whether or not reading an unpremultiplied KTX file accurately
 * creates a premultiplied buffer...
 */
DEF_TEST(KtxReadUnpremul, reporter) {

    static const uint8_t kHalfWhiteKTX[] = {
        0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, // First twelve bytes is magic
        0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A, // KTX identifier string
        0x01, 0x02, 0x03, 0x04, // Then magic endian specifier
        0x01, 0x14, 0x00, 0x00, // uint32_t fGLType;
        0x01, 0x00, 0x00, 0x00, // uint32_t fGLTypeSize;
        0x08, 0x19, 0x00, 0x00, // uint32_t fGLFormat;
        0x58, 0x80, 0x00, 0x00, // uint32_t fGLInternalFormat;
        0x08, 0x19, 0x00, 0x00, // uint32_t fGLBaseInternalFormat;
        0x02, 0x00, 0x00, 0x00, // uint32_t fPixelWidth;
        0x02, 0x00, 0x00, 0x00, // uint32_t fPixelHeight;
        0x00, 0x00, 0x00, 0x00, // uint32_t fPixelDepth;
        0x00, 0x00, 0x00, 0x00, // uint32_t fNumberOfArrayElements;
        0x01, 0x00, 0x00, 0x00, // uint32_t fNumberOfFaces;
        0x01, 0x00, 0x00, 0x00, // uint32_t fNumberOfMipmapLevels;
        0x00, 0x00, 0x00, 0x00, // uint32_t fBytesOfKeyValueData;
        0x10, 0x00, 0x00, 0x00, // image size: 2x2 image of RGBA = 4 * 4 = 16 bytes
        0xFF, 0xFF, 0xFF, 0x80, // Pixel 1
        0xFF, 0xFF, 0xFF, 0x80, // Pixel 2
        0xFF, 0xFF, 0xFF, 0x80, // Pixel 3
        0xFF, 0xFF, 0xFF, 0x80};// Pixel 4

    SkAutoTUnref<SkMemoryStream> stream(
        SkNEW_ARGS(SkMemoryStream, (kHalfWhiteKTX, sizeof(kHalfWhiteKTX))));
    REPORTER_ASSERT(reporter, NULL != stream);

    SkBitmap decodedBitmap;
    bool imageDecodeSuccess = SkImageDecoder::DecodeStream(stream, &decodedBitmap);
    REPORTER_ASSERT(reporter, imageDecodeSuccess);

    REPORTER_ASSERT(reporter, decodedBitmap.config() == SkBitmap::kARGB_8888_Config);
    REPORTER_ASSERT(reporter, decodedBitmap.alphaType() == kPremul_SkAlphaType);
    REPORTER_ASSERT(reporter, decodedBitmap.width() == 2);
    REPORTER_ASSERT(reporter, decodedBitmap.height() == 2);
    REPORTER_ASSERT(reporter, !(decodedBitmap.empty()));

    uint8_t *decodedPixels = reinterpret_cast<uint8_t*>(decodedBitmap.getPixels());
    REPORTER_ASSERT(reporter, NULL != decodedPixels);

    uint8_t *row = decodedPixels;
    for (int j = 0; j < decodedBitmap.height(); ++j) {
        for (int i = 0; i < decodedBitmap.width(); ++i) {
            SkPMColor pixel = *(reinterpret_cast<SkPMColor*>(row + i*sizeof(SkPMColor)));
            REPORTER_ASSERT(reporter, SkPreMultiplyARGB(0x80, 0xFF, 0xFF, 0xFF) == pixel);
        }
        row += decodedBitmap.rowBytes();
    }
}

/**
 * Finally, make sure that if we get ETC1 data from a PKM file that we can then
 * accurately write it out into a KTX file (i.e. transferring the ETC1 data from
 * the PKM to the KTX should produce an identical KTX to the one we have on file)
 */
DEF_TEST(KtxReexportPKM, reporter) {
    SkString resourcePath = skiatest::Test::GetResourcePath();
    SkString filename = SkOSPath::SkPathJoin(resourcePath.c_str(), "mandrill_128.pkm");

    // Load PKM file into a bitmap
    SkBitmap etcBitmap;
    SkAutoTUnref<SkData> fileData(SkData::NewFromFileName(filename.c_str()));
    REPORTER_ASSERT(reporter, NULL != fileData);

    bool installDiscardablePixelRefSuccess =
        SkInstallDiscardablePixelRef(
            SkDecodingImageGenerator::Create(
                fileData, SkDecodingImageGenerator::Options()), &etcBitmap);
    REPORTER_ASSERT(reporter, installDiscardablePixelRefSuccess);

    // Write the bitmap out to a KTX file.
    SkData *ktxDataPtr = SkImageEncoder::EncodeData(etcBitmap, SkImageEncoder::kKTX_Type, 0);
    SkAutoDataUnref newKtxData(ktxDataPtr);
    REPORTER_ASSERT(reporter, NULL != ktxDataPtr);

    // See is this data is identical to data in existing ktx file.
    SkString ktxFilename = SkOSPath::SkPathJoin(resourcePath.c_str(), "mandrill_128.ktx");
    SkAutoDataUnref oldKtxData(SkData::NewFromFileName(ktxFilename.c_str()));
    REPORTER_ASSERT(reporter, oldKtxData->equals(newKtxData));
}
