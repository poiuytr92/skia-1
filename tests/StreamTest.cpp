
/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "Test.h"
#include "SkRandom.h"
#include "SkStream.h"
#include "SkData.h"

#ifndef SK_BUILD_FOR_WIN
#include <unistd.h>
#include <fcntl.h>
#endif

#define MAX_SIZE    (256 * 1024)

static void random_fill(SkMWCRandom& rand, void* buffer, size_t size) {
    char* p = (char*)buffer;
    char* stop = p + size;
    while (p < stop) {
        *p++ = (char)(rand.nextU() >> 8);
    }
}

static void test_buffer(skiatest::Reporter* reporter) {
    SkMWCRandom rand;
    SkAutoMalloc am(MAX_SIZE * 2);
    char* storage = (char*)am.get();
    char* storage2 = storage + MAX_SIZE;

    random_fill(rand, storage, MAX_SIZE);

    for (int sizeTimes = 0; sizeTimes < 100; sizeTimes++) {
        int size = rand.nextU() % MAX_SIZE;
        if (size == 0) {
            size = MAX_SIZE;
        }
        for (int times = 0; times < 100; times++) {
            int bufferSize = 1 + (rand.nextU() & 0xFFFF);
            SkMemoryStream mstream(storage, size);
            SkBufferStream bstream(&mstream, bufferSize);

            int bytesRead = 0;
            while (bytesRead < size) {
                int s = 17 + (rand.nextU() & 0xFFFF);
                int ss = bstream.read(storage2, s);
                REPORTER_ASSERT(reporter, ss > 0 && ss <= s);
                REPORTER_ASSERT(reporter, bytesRead + ss <= size);
                REPORTER_ASSERT(reporter,
                                memcmp(storage + bytesRead, storage2, ss) == 0);
                bytesRead += ss;
            }
            REPORTER_ASSERT(reporter, bytesRead == size);
        }
    }
}

static void TestRStream(skiatest::Reporter* reporter) {
    static const char s[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    char            copy[sizeof(s)];
    SkMWCRandom        rand;

    for (int i = 0; i < 65; i++) {
        char*           copyPtr = copy;
        SkMemoryStream  mem(s, sizeof(s));
        SkBufferStream  buff(&mem, i);

        do {
            copyPtr += buff.read(copyPtr, rand.nextU() & 15);
        } while (copyPtr < copy + sizeof(s));
        REPORTER_ASSERT(reporter, copyPtr == copy + sizeof(s));
        REPORTER_ASSERT(reporter, memcmp(s, copy, sizeof(s)) == 0);
    }
    test_buffer(reporter);
}

static void test_loop_stream(skiatest::Reporter* reporter, SkStream* stream,
                             const void* src, size_t len, int repeat) {
    SkAutoSMalloc<256> storage(len);
    void* tmp = storage.get();

    for (int i = 0; i < repeat; ++i) {
        size_t bytes = stream->read(tmp, len);
        REPORTER_ASSERT(reporter, bytes == len);
        REPORTER_ASSERT(reporter, !memcmp(tmp, src, len));
    }

    // expect EOF
    size_t bytes = stream->read(tmp, 1);
    REPORTER_ASSERT(reporter, 0 == bytes);
}

static void test_filestreams(skiatest::Reporter* reporter, const char* tmpDir) {
    SkString path;
    path.printf("%s%s", tmpDir, "wstream_test");

    const char s[] = "abcdefghijklmnopqrstuvwxyz";

    {
        SkFILEWStream writer(path.c_str());
        if (!writer.isValid()) {
            SkString msg;
            msg.printf("Failed to create tmp file %s\n", path.c_str());
            reporter->reportFailed(msg.c_str());
            return;
        }

        for (int i = 0; i < 100; ++i) {
            writer.write(s, 26);
        }
    }

    {
        SkFILEStream stream(path.c_str());
        REPORTER_ASSERT(reporter, stream.isValid());
        test_loop_stream(reporter, &stream, s, 26, 100);
    }

#ifndef SK_BUILD_FOR_WIN
    {
        int fd = ::open(path.c_str(), O_RDONLY);
        SkFDStream stream(fd, true);
        REPORTER_ASSERT(reporter, stream.isValid());
        test_loop_stream(reporter, &stream, s, 26, 100);
    }
#endif
}

static void TestWStream(skiatest::Reporter* reporter) {
    SkDynamicMemoryWStream  ds;
    const char s[] = "abcdefghijklmnopqrstuvwxyz";
    int i;
    for (i = 0; i < 100; i++) {
        REPORTER_ASSERT(reporter, ds.write(s, 26));
    }
    REPORTER_ASSERT(reporter, ds.getOffset() == 100 * 26);
    char* dst = new char[100 * 26 + 1];
    dst[100*26] = '*';
    ds.copyTo(dst);
    REPORTER_ASSERT(reporter, dst[100*26] == '*');
//     char* p = dst;
    for (i = 0; i < 100; i++) {
        REPORTER_ASSERT(reporter, memcmp(&dst[i * 26], s, 26) == 0);
    }

    {
        SkData* data = ds.copyToData();
        REPORTER_ASSERT(reporter, 100 * 26 == data->size());
        REPORTER_ASSERT(reporter, memcmp(dst, data->data(), data->size()) == 0);
        data->unref();
    }
    delete[] dst;

    if (skiatest::Test::GetTmpDir()) {
        test_filestreams(reporter, skiatest::Test::GetTmpDir());
    }
}

static void TestPackedUInt(skiatest::Reporter* reporter) {
    // we know that packeduint tries to write 1, 2 or 4 bytes for the length,
    // so we test values around each of those transitions (and a few others)
    const size_t sizes[] = {
        0, 1, 2, 0xFC, 0xFD, 0xFE, 0xFF, 0x100, 0x101, 32767, 32768, 32769,
        0xFFFD, 0xFFFE, 0xFFFF, 0x10000, 0x10001,
        0xFFFFFD, 0xFFFFFE, 0xFFFFFF, 0x1000000, 0x1000001,
        0x7FFFFFFE, 0x7FFFFFFF, 0x80000000, 0x80000001, 0xFFFFFFFE, 0xFFFFFFFF
    };


    size_t i;
    char buffer[sizeof(sizes) * 4];

    SkMemoryWStream wstream(buffer, sizeof(buffer));
    for (i = 0; i < SK_ARRAY_COUNT(sizes); ++i) {
        bool success = wstream.writePackedUInt(sizes[i]);
        REPORTER_ASSERT(reporter, success);
    }
    wstream.flush();

    SkMemoryStream rstream(buffer, sizeof(buffer));
    for (i = 0; i < SK_ARRAY_COUNT(sizes); ++i) {
        size_t n = rstream.readPackedUInt();
        if (sizes[i] != n) {
            SkDebugf("-- %d: sizes:%x n:%x\n", i, sizes[i], n);
        }
        REPORTER_ASSERT(reporter, sizes[i] == n);
    }
}

// Test that setting an SkMemoryStream to a NULL data does not result in a crash when calling
// methods that access fData.
static void TestDereferencingData(SkMemoryStream* memStream) {
    memStream->read(NULL, 0);
    memStream->getMemoryBase();
    SkAutoDataUnref data(memStream->copyToData());
}

static void TestNullData() {
    SkData* nullData = NULL;
    SkMemoryStream memStream(nullData);
    TestDereferencingData(&memStream);

    memStream.setData(nullData);
    TestDereferencingData(&memStream);

}

static void TestStreams(skiatest::Reporter* reporter) {
    TestRStream(reporter);
    TestWStream(reporter);
    TestPackedUInt(reporter);
    TestNullData();
}

#include "TestClassDef.h"
DEFINE_TESTCLASS("Stream", StreamTestClass, TestStreams)
