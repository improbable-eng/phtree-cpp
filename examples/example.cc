/*
 * Copyright 2020 Improbable Worlds Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "../phtree/phtree.h"
#include <iostream>

using namespace improbable::phtree;

int main() {
    std::cout << "PH-Tree example with 3D `double` coordinates." << std::endl;
    PhPointD<3> p1({1, 1, 1});
    PhPointD<3> p2({2, 2, 2});
    PhPointD<3> p3({3, 3, 3});
    PhPointD<3> p4({4, 4, 4});

    PhTreeD<3, int> tree;
    tree.emplace(p1, 1);
    tree.emplace(p2, 2);
    tree.emplace(p3, 3);
    tree.emplace(p4, 4);

    std::cout << "All values:" << std::endl;
    for (auto it : tree) {
        std::cout << "    id=" << it << std::endl;
    }
    std::cout << std::endl;

    std::cout << "All points in range:" << p2 << "/" << p4 << std::endl;
    for (auto it = tree.begin_query({p2, p4}); it != tree.end(); ++it) {
        std::cout << "    " << it.second() << " -> " << it.first() << std::endl;
    }
    std::cout << std::endl;

    std::cout << "PH-Tree is a MAP which means that, like std::map, every position " << std::endl;
    std::cout << " (=key) can have only ONE value." << std::endl;
    std::cout << "Storing multiple values for a single coordinate requires storing " << std::endl;
    std::cout << "lists or sets, for example using PhTree<3, std::vector<int>>." << std::endl;

    PhPointD<3> p4b({4, 4, 4});
    tree.emplace(p4b, 5);
    // Still showing '4' after emplace()
    std::cout << "ID at " << p4b << ": " << tree.find(p4b).second() << std::endl;

    std::cout << "Done." << std::endl;
}