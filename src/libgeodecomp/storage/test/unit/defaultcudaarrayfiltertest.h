#include <libgeodecomp/config.h>

// Kill some warnings in system headers
#ifdef _MSC_BUILD
#pragma warning( push )
#pragma warning( disable : 4514 4710 4711 )
#endif

#include <cxxtest/TestSuite.h>
#include <vector>

#ifdef LIBGEODECOMP_WITH_CUDA
#include <cuda.h>
#include <libflatarray/cuda_array.hpp>
#endif

#ifdef _MSC_BUILD
#pragma warning( pop )
#endif

#ifdef LIBGEODECOMP_WITH_HPX
#include <libgeodecomp/communication/hpxserialization.h>
#endif

#include <libgeodecomp/misc/testcell.h>
#include <libgeodecomp/storage/defaultcudaarrayfilter.h>

using namespace LibGeoDecomp;

namespace LibGeoDecomp {

class MyDumbestSoACell
{
public:
    class API :
        public APITraits::HasSoA
    {};

    explicit MyDumbestSoACell(
        const int x = 0,
        const double y1 = 0) :
        x(x)
    {
        for (int i = 0; i < 256; ++i) {
            y[i] = y1;
        }
    }

    int x;
    double y[256];
};

}

LIBFLATARRAY_REGISTER_SOA(LibGeoDecomp::MyDumbestSoACell, ((int)(x))((double)(y)(256)) )

namespace LibGeoDecomp {

class DefaultCUDAArrayFilterTest : public CxxTest::TestSuite
{
public:
    void testCudaAoSWithGridOnDeviceAndBuffersOnDevice()
    {
        // TEST 1: Copy Out (Device to Device)
        LibFlatArray::cuda_array<MyDumbestSoACell> deviceCellVec(222);
        std::vector<MyDumbestSoACell> hostCellVec(222);

        LibFlatArray::cuda_array<double> deviceBuffer(222 * 256);
        std::vector<double> hostBuffer(222 * 256);

        for (std::size_t i = 0; i < hostCellVec.size(); ++i) {
            for (std::size_t j = 0; j < 256; ++j) {
                hostCellVec[i].y[j] = i + j + 0.221;
            }
        }
        deviceCellVec.load(&hostCellVec[0]);

        FilterBase<MyDumbestSoACell> *filter =
            new DefaultCUDAArrayFilter<MyDumbestSoACell, double, double, 256>();
        char MyDumbestSoACell::* memberPointer =
            reinterpret_cast<char MyDumbestSoACell::*>(&MyDumbestSoACell::y);

        filter->copyMemberOut(
            deviceCellVec.data(),
            MemoryLocation::CUDA_DEVICE,
            reinterpret_cast<char*>(deviceBuffer.data()),
            MemoryLocation::CUDA_DEVICE,
            222,
            memberPointer);
        deviceBuffer.save(&hostBuffer[0]);

        for (std::size_t i = 0; i < hostBuffer.size(); i += 256) {
            for (std::size_t j = 0; j < 256; ++j) {
                TS_ASSERT_EQUALS(i / 256 + j + 0.221, hostBuffer[i + j]);
            }
        }

        // TEST 2: Copy In (Device to Device)
        for (std::size_t i = 0; i < hostBuffer.size(); i += 256) {
            for (std::size_t j = 0; j < 256; ++j) {
                hostBuffer[i + j] = i * 1000 + j + 0.222;
            }
        }
        deviceBuffer.load(&hostBuffer[0]);

        filter->copyMemberIn(
            reinterpret_cast<char*>(deviceBuffer.data()),
            MemoryLocation::CUDA_DEVICE,
            deviceCellVec.data(),
            MemoryLocation::CUDA_DEVICE,
            222,
            memberPointer);

        deviceCellVec.save(&hostCellVec[0]);

        for (std::size_t i = 0; i < hostCellVec.size(); ++i) {
            for (std::size_t j = 0; j < 256; ++j) {
                TS_ASSERT_EQUALS(i * 1000 * 256 + j + 0.222, hostCellVec[i].y[j]);
            }
        }
    }
    void testCudaAoSWithGridOnDeviceAndBuffersOnHost()
    {
        // TEST 1: Copy Out (Device to Host)
        LibFlatArray::cuda_array<MyDumbestSoACell> deviceCellVec(333);
        std::vector<MyDumbestSoACell> hostCellVec(333);

        std::vector<double> hostBuffer(333 * 256);

        for (std::size_t i = 0; i < hostCellVec.size(); ++i) {
            for (std::size_t j = 0; j < 256; ++j) {
                hostCellVec[i].y[j] = i + j + 0.331;
            }
        }
        deviceCellVec.load(&hostCellVec[0]);

        FilterBase<MyDumbestSoACell> *filter =
            new DefaultCUDAArrayFilter<MyDumbestSoACell, double, double, 256>();
        char MyDumbestSoACell::* memberPointer =
            reinterpret_cast<char MyDumbestSoACell::*>(&MyDumbestSoACell::y);

        filter->copyMemberOut(
            deviceCellVec.data(),
            MemoryLocation::CUDA_DEVICE,
            reinterpret_cast<char*>(&hostBuffer[0]),
            MemoryLocation::HOST,
            333,
            memberPointer);

        for (std::size_t i = 0; i < hostBuffer.size(); i += 256) {
            for (std::size_t j = 0; j < 256; ++j) {
                TS_ASSERT_EQUALS(i / 256 + j + 0.331, hostBuffer[i + j]);
            }
        }

        // TEST 2: Copy In (Host to Device)
        for (std::size_t i = 0; i < hostBuffer.size(); i += 256) {
            for (std::size_t j = 0; j < 256; ++j) {
                hostBuffer[i + j] = i * 1000 + j + 0.332;
            }
        }

        filter->copyMemberIn(
            reinterpret_cast<char*>(&hostBuffer[0]),
            MemoryLocation::HOST,
            deviceCellVec.data(),
            MemoryLocation::CUDA_DEVICE,
            333,
            memberPointer);

        deviceCellVec.save(&hostCellVec[0]);

        for (std::size_t i = 0; i < hostCellVec.size(); ++i) {
            for (std::size_t j = 0; j < 256; ++j) {
                TS_ASSERT_EQUALS(i * 1000 * 256 + j + 0.332, hostCellVec[i].y[j]);
            }
        }
    }

    void testCudaAoSWithGridOnHostAndBuffersOnDevice()
    {
        // TEST 1: Copy Out (Host to Device)
        std::vector<MyDumbestSoACell> hostCellVec(444);

        LibFlatArray::cuda_array<double> deviceBuffer(444 * 256);
        std::vector<double> hostBuffer(444 * 256);

        for (std::size_t i = 0; i < hostCellVec.size(); ++i) {
            for (std::size_t j = 0; j < 256; ++j) {
                hostCellVec[i].y[j] = i + j + 0.441;
            }
        }

        FilterBase<MyDumbestSoACell> *filter =
            new DefaultCUDAArrayFilter<MyDumbestSoACell, double, double, 256>();
        char MyDumbestSoACell::* memberPointer =
            reinterpret_cast<char MyDumbestSoACell::*>(&MyDumbestSoACell::y);

        filter->copyMemberOut(
            &hostCellVec[0],
            MemoryLocation::HOST,
            reinterpret_cast<char*>(deviceBuffer.data()),
            MemoryLocation::CUDA_DEVICE,
            444,
            memberPointer);
        deviceBuffer.save(&hostBuffer[0]);

        for (std::size_t i = 0; i < hostBuffer.size(); i += 256) {
            for (std::size_t j = 0; j < 256; ++j) {
                TS_ASSERT_EQUALS(i / 256 + j + 0.441, hostBuffer[i + j]);
            }
        }

        // TEST 2: Copy In (Device to Host)
        for (std::size_t i = 0; i < hostBuffer.size(); i += 256) {
            for (std::size_t j = 0; j < 256; ++j) {
                hostBuffer[i + j] = i * 1000 + j + 0.442;
            }
        }
        deviceBuffer.load(&hostBuffer[0]);

        filter->copyMemberIn(
            reinterpret_cast<char*>(deviceBuffer.data()),
            MemoryLocation::CUDA_DEVICE,
            &hostCellVec[0],
            MemoryLocation::HOST,
            444,
            memberPointer);

        for (std::size_t i = 0; i < hostCellVec.size(); ++i) {
            for (std::size_t j = 0; j < 256; ++j) {
                TS_ASSERT_EQUALS(i * 1000 * 256 + j + 0.442, hostCellVec[i].y[j]);
            }
        }
    }

    void testCudaAoSWithGridOnHostAndBuffersOnHost()
    {
        std::vector<MyDumbestSoACell> hostCellVec(111);
        std::vector<double> hostBuffer(111 * 256);

        for (std::size_t i = 0; i < hostCellVec.size(); ++i) {
            for (std::size_t j = 0; j < 256; ++j) {
                hostCellVec[i].y[j] = i + j + 0.111;
            }
        }

        FilterBase<MyDumbestSoACell> *filter =
            new DefaultCUDAArrayFilter<MyDumbestSoACell, double, double, 256>();
        char MyDumbestSoACell::* memberPointer =
            reinterpret_cast<char MyDumbestSoACell::*>(&MyDumbestSoACell::y);

        filter->copyMemberOut(
            &hostCellVec[0],
            MemoryLocation::HOST,
            reinterpret_cast<char*>(&hostBuffer[0]),
            MemoryLocation::HOST,
            111,
            memberPointer);

        for (std::size_t i = 0; i < hostBuffer.size(); i += 256) {
            for (std::size_t j = 0; j < 256; ++j) {
                TS_ASSERT_EQUALS(i / 256 + j + 0.111, hostBuffer[i + j]);
            }
        }

        for (std::size_t i = 0; i < hostBuffer.size(); i += 256) {
            for (std::size_t j = 0; j < 256; ++j) {
                hostBuffer[i + j] = i * 1000 + j + 0.112;
            }
        }

        filter->copyMemberIn(
            reinterpret_cast<char*>(&hostBuffer[0]),
            MemoryLocation::HOST,
            &hostCellVec[0],
            MemoryLocation::HOST,
            111,
            memberPointer);

        for (std::size_t i = 0; i < hostCellVec.size(); ++i) {
            for (std::size_t j = 0; j < 256; ++j) {
                TS_ASSERT_EQUALS(i * 1000 * 256 + j + 0.112, hostCellVec[i].y[j]);
            }
        }
    }

    void testCudaSoAWithGridOnDeviceAndBuffersOnDevice()
    {
        // elements of a member array are split up according to a
        // certain stride (in this case 6666). In an external buffer
        // (hostBuffer) these array elements will be aggregated and
        // stored directly one after another:

        // TEST 1: Copy Out (Device to Device)
        std::vector<double> hostMemberVec(256 * 6666, -1);
        LibFlatArray::cuda_array<double> deviceMemberVec(&hostMemberVec[0], 256 * 6666);

        std::vector<double> hostBuffer(256 * 6666, -2);
        LibFlatArray::cuda_array<double> deviceBuffer(&hostBuffer[0], 256 * 6666);

        for (std::size_t i = 0; i < 6666; ++i) {
            for (std::size_t j = 0; j < 256; ++j) {
                hostMemberVec[i + j * 6666] = i * 1000 + j + 0.6661;
            }
        }

        deviceMemberVec.load(&hostMemberVec[0]);

        FilterBase<MyDumbestSoACell > *filter =
            new DefaultCUDAArrayFilter<MyDumbestSoACell, double, double, 256>();

        filter->copyStreakOut(
            reinterpret_cast<char*>(deviceMemberVec.data()),
            MemoryLocation::CUDA_DEVICE,
            reinterpret_cast<char*>(deviceBuffer.data()),
            MemoryLocation::CUDA_DEVICE,
            6666,
            6666);

        deviceBuffer.save(&hostBuffer[0]);

        for (std::size_t i = 0; i < hostBuffer.size(); i += 256) {
            for (std::size_t j = 0; j < 256; ++j) {
                TS_ASSERT_EQUALS(i / 256 * 1000 + j + 0.6661, hostBuffer[i + j]);
            }
        }

        // TEST 2: Copy In (Device to Device)
        for (std::size_t i = 0; i < hostBuffer.size(); i += 256) {
            for (std::size_t j = 0; j < 256; ++j) {
                hostBuffer[i + j] = i / 256 * 1000 + j + 0.6662;
            }
        }
        deviceBuffer.load(&hostBuffer[0]);

        filter->copyStreakIn(
            reinterpret_cast<char*>(deviceBuffer.data()),
            MemoryLocation::CUDA_DEVICE,
            reinterpret_cast<char*>(deviceMemberVec.data()),
            MemoryLocation::CUDA_DEVICE,
            6666,
            6666);

        deviceMemberVec.save(&hostMemberVec[0]);

        for (std::size_t i = 0; i < 6666; ++i) {
            for (std::size_t j = 0; j < 256; ++j) {
                TS_ASSERT_EQUALS(i * 1000 + j + 0.6662, hostMemberVec[i + j * 6666]);
            }
        }
    }

    void testCudaSoAWithGridOnDeviceAndBuffersOnHost()
    {
        // elements of a member array are split up according to a
        // certain stride (in this case 7777). In an external buffer
        // (hostBuffer) these array elements will be aggregated and
        // stored directly one after another:

        // TEST 1: Copy Out (Device to Host)
        std::vector<double> hostMemberVec(256 * 7777, -1);
        LibFlatArray::cuda_array<double> deviceMemberVec(&hostMemberVec[0], 256 * 7777);

        std::vector<double> hostBuffer(256 * 7777, -2);

        for (std::size_t i = 0; i < 7777; ++i) {
            for (std::size_t j = 0; j < 256; ++j) {
                hostMemberVec[i + j * 7777] = i * 1000 + j + 0.7771;
            }
        }

        deviceMemberVec.load(&hostMemberVec[0]);

        FilterBase<MyDumbestSoACell > *filter =
            new DefaultCUDAArrayFilter<MyDumbestSoACell, double, double, 256>();

        filter->copyStreakOut(
            reinterpret_cast<char*>(deviceMemberVec.data()),
            MemoryLocation::CUDA_DEVICE,
            reinterpret_cast<char*>(&hostBuffer[0]),
            MemoryLocation::HOST,
            7777,
            7777);

        for (std::size_t i = 0; i < hostBuffer.size(); i += 256) {
            for (std::size_t j = 0; j < 256; ++j) {
                TS_ASSERT_EQUALS(i / 256 * 1000 + j + 0.7771, hostBuffer[i + j]);
            }
        }

        // TEST 2: Copy In (Host to Device)
        for (std::size_t i = 0; i < hostBuffer.size(); i += 256) {
            for (std::size_t j = 0; j < 256; ++j) {
                hostBuffer[i + j] = i / 256 * 1000 + j + 0.7772;
            }
        }

        filter->copyStreakIn(
            reinterpret_cast<char*>(&hostBuffer[0]),
            MemoryLocation::HOST,
            reinterpret_cast<char*>(deviceMemberVec.data()),
            MemoryLocation::CUDA_DEVICE,
            7777,
            7777);

        deviceMemberVec.save(&hostMemberVec[0]);

        for (std::size_t i = 0; i < 7777; ++i) {
            for (std::size_t j = 0; j < 256; ++j) {
                TS_ASSERT_EQUALS(i * 1000 + j + 0.7772, hostMemberVec[i + j * 7777]);
            }
        }
    }

    void testCudaSoAWithGridOnHostAndBuffersOnDevice()
    {
        // elements of a member array are split up according to a
        // certain stride (in this case 8888). In an external buffer
        // (hostBuffer) these array elements will be aggregated and
        // stored directly one after another:

        // TEST 1: Copy Out (Host to Device)
        std::vector<double> hostMemberVec(256 * 8888, -1);

        std::vector<double> hostBuffer(256 * 8888, -2);
        LibFlatArray::cuda_array<double> deviceBuffer(&hostBuffer[0], 256 * 8888);

        for (std::size_t i = 0; i < 8888; ++i) {
            for (std::size_t j = 0; j < 256; ++j) {
                hostMemberVec[i + j * 8888] = i * 1000 + j + 0.8881;
            }
        }

        FilterBase<MyDumbestSoACell > *filter =
            new DefaultCUDAArrayFilter<MyDumbestSoACell, double, double, 256>();

        filter->copyStreakOut(
            reinterpret_cast<char*>(&hostMemberVec[0]),
            MemoryLocation::HOST,
            reinterpret_cast<char*>(deviceBuffer.data()),
            MemoryLocation::CUDA_DEVICE,
            8888,
            8888);

        deviceBuffer.save(&hostBuffer[0]);

        for (std::size_t i = 0; i < hostBuffer.size(); i += 256) {
            for (std::size_t j = 0; j < 256; ++j) {
                TS_ASSERT_EQUALS(i / 256 * 1000 + j + 0.8881, hostBuffer[i + j]);
            }
        }

        // TEST 2: Copy In (Device to Host)
        for (std::size_t i = 0; i < hostBuffer.size(); i += 256) {
            for (std::size_t j = 0; j < 256; ++j) {
                hostBuffer[i + j] = i / 256 * 1000 + j + 0.8882;
            }
        }
        deviceBuffer.load(&hostBuffer[0]);

        filter->copyStreakIn(
            reinterpret_cast<char*>(deviceBuffer.data()),
            MemoryLocation::CUDA_DEVICE,
            reinterpret_cast<char*>(&hostMemberVec[0]),
            MemoryLocation::HOST,
            8888,
            8888);

        for (std::size_t i = 0; i < 8888; ++i) {
            for (std::size_t j = 0; j < 256; ++j) {
                TS_ASSERT_EQUALS(i * 1000 + j + 0.8882, hostMemberVec[i + j * 8888]);
            }
        }
    }

    void testCudaSoAWithGridOnHostAndBuffersOnHost()
    {
        // elements of a member array are split up according to a
        // certain stride (in this case 5555). In an external buffer
        // (hostBuffer) these array elements will be aggregated and
        // stored directly one after another:

        // TEST 1: Copy Out (Host to Host)
        std::vector<double> hostMemberVec(256 * 5555);
        std::vector<double> hostBuffer(256 * 5555, -1);

        for (std::size_t i = 0; i < 5555; ++i) {
            for (std::size_t j = 0; j < 256; ++j) {
                hostMemberVec[i + j * 5555] = i * 1000 + j + 0.5551;
            }
        }

        FilterBase<MyDumbestSoACell > *filter =
            new DefaultCUDAArrayFilter<MyDumbestSoACell, double, double, 256>();

        filter->copyStreakOut(
            reinterpret_cast<char*>(&hostMemberVec[0]),
            MemoryLocation::HOST,
            reinterpret_cast<char*>(&hostBuffer[0]),
            MemoryLocation::HOST,
            5555,
            5555);

        for (std::size_t i = 0; i < hostBuffer.size(); i += 256) {
            for (std::size_t j = 0; j < 256; ++j) {
                TS_ASSERT_EQUALS(i / 256 * 1000 + j + 0.5551, hostBuffer[i + j]);
            }
        }

        // TEST 2: Copy In (Host to Host)
        for (std::size_t i = 0; i < hostBuffer.size(); i += 256) {
            for (std::size_t j = 0; j < 256; ++j) {
                hostBuffer[i + j] = i / 256 * 1000 + j + 0.5552;
            }
        }

        filter->copyStreakIn(
            reinterpret_cast<char*>(&hostBuffer[0]),
            MemoryLocation::HOST,
            reinterpret_cast<char*>(&hostMemberVec[0]),
            MemoryLocation::HOST,
            5555,
            5555);

        for (std::size_t i = 0; i < 5555; ++i) {
            for (std::size_t j = 0; j < 256; ++j) {
                TS_ASSERT_EQUALS(i * 1000 + j + 0.5552, hostMemberVec[i + j * 5555]);
            }
        }
    }
};

}
