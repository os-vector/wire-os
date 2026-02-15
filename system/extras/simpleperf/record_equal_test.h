/*
 * Copyright (C) 2015 The Android Open Source Project
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

static void CheckMmapRecordDataEqual(const MmapRecord& r1, const MmapRecord& r2) {
  ASSERT_EQ(0, memcmp(&r1.data, &r2.data, sizeof(r1.data)));
  ASSERT_EQ(r1.filename, r2.filename);
}

static void CheckCommRecordDataEqual(const CommRecord& r1, const CommRecord& r2) {
  ASSERT_EQ(0, memcmp(&r1.data, &r2.data, sizeof(r1.data)));
  ASSERT_EQ(r1.comm, r2.comm);
}

static void CheckBuildIdRecordDataEqual(const BuildIdRecord& r1, const BuildIdRecord& r2) {
  ASSERT_EQ(r1.pid, r2.pid);
  ASSERT_EQ(r1.build_id, r2.build_id);
  ASSERT_EQ(r1.filename, r2.filename);
}

static void CheckRecordEqual(const Record& r1, const Record& r2) {
  ASSERT_EQ(0, memcmp(&r1.header, &r2.header, sizeof(r1.header)));
  ASSERT_EQ(0, memcmp(&r1.sample_id, &r2.sample_id, sizeof(r1.sample_id)));
  if (r1.header.type == PERF_RECORD_MMAP) {
    CheckMmapRecordDataEqual(static_cast<const MmapRecord&>(r1), static_cast<const MmapRecord&>(r2));
  } else if (r1.header.type == PERF_RECORD_COMM) {
    CheckCommRecordDataEqual(static_cast<const CommRecord&>(r1), static_cast<const CommRecord&>(r2));
  } else if (r1.header.type == PERF_RECORD_BUILD_ID) {
    CheckBuildIdRecordDataEqual(static_cast<const BuildIdRecord&>(r1),
                                static_cast<const BuildIdRecord&>(r2));
  }
}
