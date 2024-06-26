/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef _INIT_PERSISTENT_PROPERTIES_H
#define _INIT_PERSISTENT_PROPERTIES_H

#include <string>
#include <vector>


namespace propd {

std::vector<std::pair<std::string, std::string>> LoadPersistentProperties();
void WritePersistentProperty(const std::string& name, const std::string& value);

}  // namespace propd

#endif
