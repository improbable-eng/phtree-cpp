[![Build status](TODO)](TODO)

# PH-Tree C++

The PH-Tree is an ordered index on an n-dimensional space (quad-/oct-/2^n-tree) where each
dimension is (by default) indexed by a 64 bit integer. The index order follows z-order / Morton
order. The default implementation is effectively a 'map', i.e. each key is associated with at most one value.
Keys are points or boxes in n-dimensional space.



One strength of PH-Trees are fast insert/removal operations and scalability with large datasets.
It also provides fast window queries and _k_-nearest neighbor queries, and it scales well with higher dimenions.
The default implementation is limited to 63 dimensions.

The API ist mostly analogous to STL's `std::map`, see function descriptions for details.

See also :
- T. Zaeschke, C. Zimmerli, M.C. Norrie:
  "The PH-Tree -- A Space-Efficient Storage Structure and Multi-Dimensional Index", (SIGMOD 2014)
- T. Zaeschke: "The PH-Tree Revisited", (2015)
- T. Zaeschke, M.C. Norrie: "Efficient Z-Ordered Traversal of Hypercube Indexes" (BTW 2017).

More information about PH-Trees (including a Java implementation) is available [here](http://www.phtree.org). 


## Usage 

#### Key Types

The PH-Tree supports three types of keys:
- `PhTreeD` uses `PhPointD` keys, which are vectors/points of 64 bit `double`. 
- `PhTreeBoxD` uses `PhBoxD` keys, which consist of two `PhPointD` that define an axis-aligned rectangle/box.
- `PhTree` uses `PhPoint` keys, which are vectors/points of `std::int64` 

#### Basic Operations
```C++
class MyData { ... };
MyData my_data; 

// Create a 3D point tree with floating point coordinates and a value type of `MyData`.
auto tree = PhTreeD<3, MyData>();

// Create coordinate
PhPointD<3> p{1.1, 1.0, 10.};

// Some operations
tree.emplace(p, my_data);
tree.insert(p, my_data);
tree[p] = my_data;
tree.count(p);
tree.find(p);
tree.erase(p);
tree.size();
tree.empty();
tree.clear();
```

#### Queries

* Iterator over all elements: `auto q = tree.begin();`
* Iterator for box shaped window queries: `auto q = tree.begin_query(min, max);`
* Iterator for _k_ nearest neighbor queries: `auto q = tree.begin_knn_query(k, center_point);`

```C++
// Iterate over all entries
for (auto it : tree) {
    ...
}

// Iterate over all entries inside of an axis aligned box defined by the two points (1,1,1) and (3,3,3)    
for (auto it = tree.begin_query({1, 1, 1}, {3, 3, 3}); it != tree.end(); ++it) {
    ...
}

// Find 5 nearest neighbors of (1,1,1)    
for (auto it = tree.begin_knn_query(5, {1, 1, 1}); it != tree.end(); ++it) {
    ...
}
```

All queries allow specifying an additional filter. The filter is called for every key/value pair that the would 
normally be returned (subject to query constraints) and to every node in the tree that the query decides to 
traverse (also subject to query constraints). Returning `true` in the filter does not change query behaviour, 
returning `false` means that the current value or child node is not returned or traversed.
An example of a geometric filter can be found in `phtree/common/ph_filter.h` in `PhFilterAABB`.
```C++
template <dimension_t DIM, typename T>
struct FilterByValueId {
    [[nodiscard]] constexpr bool IsEntryValid(const PhPoint<DIM>& key, const T& value) const {
        // Arbitrary example: Only allow values with even values of id_
        return value.id_ % 2 == 0;
    }
    [[nodiscard]] constexpr bool IsNodeValid(const PhPoint<DIM>& prefix, int bits_to_ignore) const {
        // Allow all nodes
        return true;
    }
};

// Iterate over all entries inside of an axis aligned box defined by the two points (1,1,1) and (3,3,3).
// Return only entries that suffice the filter condition.    
for (auto it = tree.begin_query({1, 1, 1}, {3, 3, 3}, FilterByValueId<3, T>())); it != tree.end(); ++it) {
    ...
}
```

Nearest neighbor queries can also use custom distance metrics, such as L1 distance.
Note that this returns a special iterator that provides a function to get the distance of the
current entry:
```C++
#include "phtree/phtree_d.h"

// Find 5 nearest neighbors of (1,1,1) using L1 distance    
for (auto it = tree.begin_knn_query(5, {1, 1, 1}, PhDistanceLongL1<3>())); it != tree.end(); ++it) {
    std::cout << "distance = " << it.distance() << std::endl;
    ...
}

```

#### Pre- & Post-Processors
The PH-Tree can internally only process integer keys. In order to use floating point coordinates, the floating point 
coordinates must be converted to integer coordinates. The `PhTreeD` and `PhTreeBoxD` use by default the 
`PreprocessIEEE` & `PostProcessIEEE` functions. The `IEEE` processor is a loss-less converter (in term of numeric 
precision) that simply takes the 64bits of a double value and treats them as if they were a 64bit integer 
(it is slightly more complicated than that, see discussion in the papers referenced above).
In other words, it treats the IEEE 754 representation of the double value as integer, hence the name `IEEE` converter.

The `IEEE` conversion is fast and reversible without loss of precision. However, it has been shown that other
converters can result in indexes that are up to 20% faster. 
One useful alternative is a `Multiply` converter that convert floating point to integer by multiplication 
and casting:
```C++
double my_float = ...;
// Convert to int 
std::int64_t my_int = (std::int64_t) my_float * 1000000.;

// Convert back
double resultung_float = ((double)my_int) / 1000000.;  
```
It is obvious that this approach leads to a loss of numerical precision. Moreover, the loss of precision depends
on the actual range of the double values and the constant.
The chosen constant should probably be as large as possible, but small enough such that converted 
values do not exceed the 64bit limit of `std::int64_4`.

```C++
static const double MY_MULTIPLIER = 1000000.;
static const double MY_DIVIDER = 1./MY_MULTIPLIER;

template <dimension_t DIM>
PhPoint<DIM> PreprocessMultiply(const PhPointD<DIM>& point) {
    PhPoint<DIM> out;
    for (dimension_t i = 0; i < DIM; ++i) {
        out[i] = point[i] * MY_MULTIPLIER;
    }
    return out;
}

template <dimension_t DIM>
PhPointD<DIM> PostprocessMultiply(const PhPoint<DIM>& in) {
    PhPointD<DIM> out;
    for (dimension_t i = 0; i < DIM; ++i) {
        out[i] = ((double)in[i]) * MY_DIVIDER;
    }
    return out;
}

template <dimension_t DIM, typename T>
using MyTree = PhTreeD<DIM, T, TestPoint<DIM>, PreprocessMultiply<DIM>, PostprocessMultiply<DIM>>;
```  

It is also worth trying out constants that are 1 or 2 orders of magnitude smaller or larger than this maximum value. 
Experience shows that this may affect query performance by up to 10%. The reason for this is currently unknown.  




#### Restrictions

* **C++**: Supports value types of `T` and `T*`, but not `T&`
* **C++**: Return types of `find()`, `emplace()`, ... differ slightly from `std::map`, they have function `first()`, `second()` instead of fields of the same name.
* **General**: PH-Trees are **maps**, i.e. each coordinate can hold only *one* entry. In order to hold multiple coordinates per entry, one needs to insert lists or hashmaps as values.
* **General**: PH-Trees order entries internally in z-order (Morton order). However, the order is based on the (unsigned) bit represenation of keys, so negative coordinates are returned *after* positive coordinates.
* **General**: The current implementation support between 2 and 63 dimensions.
* **Differences to std::map**: There are several differences to `std::map`. Most notably for the iterators:
  * `begin()`/`end()` are not comparable with `<` or `>`. Only `it == tree.end()` and `it != tree.end()` is supported. 
  * Value of `end()`: The tree has no linear memory layout, so there is no useful definition of a pointer pointing _after_ the last entry or any entry. This should be irrelevant for normal usage.
 

## Compiling the PH-Tree

This section will guide you through the initial build system and IDE you need to go through in order to build and run custom versions of the PH-Tree on your machine.

### Build system & dependencies

PH-Tree can be built with *cmake 3.14* or [Bazel](https://bazel.build) as build system. All of the code is written in C++ targeting the C++17 standard.
The code has been verified to compile with Clang 9 on Linux and Visual Studio 2019 on Windows.

#### Ubuntu Linux

Installing clang & bazel:
```
echo "deb [arch=amd64] https://storage.googleapis.com/bazel-apt stable jdk1.8" | sudo tee /etc/apt/sources.list.d/bazel.list
curl https://bazel.build/bazel-release.pub.gpg | sudo apt-key add -
curl https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
sudo apt-add-repository 'deb http://apt.llvm.org/bionic/ llvm-toolchain-bionic-9 main'
sudo apt-get update
sudo apt-get install clang-9 bazel
```

To install [*cmake*](https://launchpad.net/~hnakamur/+archive/ubuntu/cmake):
```
sudo add-apt-repository ppa:hnakamur/libarchive
sudo add-apt-repository ppa:hnakamur/libzstd
sudo add-apt-repository ppa:hnakamur/cmake
sudo apt update
sudo apt install cmake
```

#### Windows

To build on Windows, you'll need to have a version of Visual Studio 2019 installed (likely Professional), in addition to the latest version of
[Bazel](https://docs.bazel.build/versions/master/windows.html).
   

### Bazel
Once you have set up your dependencies, you should be able to build the PH-Tree repository by running:
```
bazel build ...
```

Similarly, you can run all unit tests with:
```
bazel test ...
```

### cmake
```
mkdir build
cd build
cmake ..
cmake --build .
./example/Example
```

## Troubleshooting

**Problem**: The PH-Tree appears to be losing updates/insertions.
**Solution**: Remember that the PH-Tree is a *map*, keys will not be inserted if an identical key already exists.
