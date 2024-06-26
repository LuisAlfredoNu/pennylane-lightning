// Copyright 2018-2023 Xanadu Quantum Technologies Inc.

// Licensed under the Apache License, Version 2.0 (the License);
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

// http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an AS IS BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <algorithm>
#include <complex>
#include <cstdio>
#include <vector>

#include <catch2/catch.hpp>

#include "DataBuffer.hpp"
#include "LinearAlg.hpp"
#include "TestHelpers.hpp"
#include "Util.hpp" // exp2
#include "cuda_helpers.hpp"

/**
 * @file
 *  Tests linear algebra functionality defined for the class
 * StateVectorCudaManaged.
 */

/// @cond DEV
namespace {
using namespace Pennylane::LightningGPU;
using namespace Pennylane::LightningGPU::Util;
using namespace Pennylane::Util;
} // namespace
/// @endcond

TEMPLATE_TEST_CASE("Linear Algebra::SparseMV", "[Linear Algebra]", float,
                   double) {
    using ComplexT = std::complex<TestType>;
    using IdxT = typename std::conditional<std::is_same<TestType, float>::value,
                                           int32_t, int64_t>::type;

    using CFP_t =
        typename std::conditional<std::is_same<TestType, float>::value,
                                  cuFloatComplex, cuDoubleComplex>::type;

    std::size_t num_qubits = 3;
    std::size_t data_size = exp2(num_qubits);

    std::vector<ComplexT> vectors = {{0.0, 0.0}, {0.0, 0.1}, {0.1, 0.1},
                                     {0.1, 0.2}, {0.2, 0.2}, {0.3, 0.3},
                                     {0.3, 0.4}, {0.4, 0.5}};

    std::vector<CFP_t> vectors_cu;

    std::transform(vectors.begin(), vectors.end(),
                   std::back_inserter(vectors_cu),
                   [](ComplexT x) { return complexToCu<ComplexT>(x); });

    const std::vector<ComplexT> result_refs = {
        {0.2, -0.1}, {-0.1, 0.2}, {0.2, 0.1}, {0.1, 0.2},
        {0.7, -0.2}, {-0.1, 0.6}, {0.6, 0.1}, {0.2, 0.7}};

    std::vector<IdxT> indptr = {0, 2, 4, 6, 8, 10, 12, 14, 16};
    std::vector<IdxT> indices = {0, 3, 1, 2, 1, 2, 0, 3,
                                 4, 7, 5, 6, 5, 6, 4, 7};
    std::vector<ComplexT> values = {
        {1.0, 0.0},  {0.0, -1.0}, {1.0, 0.0}, {0.0, 1.0},
        {0.0, -1.0}, {1.0, 0.0},  {0.0, 1.0}, {1.0, 0.0},
        {1.0, 0.0},  {0.0, -1.0}, {1.0, 0.0}, {0.0, 1.0},
        {0.0, -1.0}, {1.0, 0.0},  {0.0, 1.0}, {1.0, 0.0}};

    DataBuffer<CFP_t> sv_x(data_size);
    DataBuffer<CFP_t> sv_y(data_size);

    sv_x.CopyHostDataToGpu(vectors_cu.data(), vectors_cu.size());

    SECTION("Testing sparse matrix vector product:") {
        std::vector<CFP_t> result(data_size);
        auto cusparsehandle = make_shared_cusparse_handle();

        SparseMV_cuSparse<IdxT, TestType, CFP_t>(
            indptr.data(), static_cast<int64_t>(indptr.size()), indices.data(),
            values.data(), static_cast<int64_t>(values.size()), sv_x.getData(),
            sv_y.getData(), sv_x.getDevice(), sv_x.getStream(),
            cusparsehandle.get());

        sv_y.CopyGpuDataToHost(result.data(), result.size());

        for (std::size_t j = 0; j < exp2(num_qubits); j++) {
            CHECK(result[j].x == Approx(real(result_refs[j])));
            CHECK(result[j].y == Approx(imag(result_refs[j])));
        }
    }
}
