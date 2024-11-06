#include <membenchmc/membenchmc.h>
#ifdef ALPAKA_ACC_GPU_CUDA_ENABLED
#  include <cuda_runtime.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>

namespace membenchmc {
  /**
   * @brief Trims leading and trailing whitespace from a string.
   *
   * @param str The string to trim.
   * @return std::string The trimmed string.
   */
  std::string trim(const std::string& str) {
    auto start = std::find_if(str.cbegin(), str.cend(),
                              [](unsigned char ch) { return !std::isspace(ch); });
    auto end = std::find_if(str.crbegin(), str.crend(), [](unsigned char ch) {
                 return !std::isspace(ch);
               }).base();
    return std::string(start, end);
  }

  /**
   * @brief Calls `lscpu` and returns its stdout as string.
   *
   * @return std::string A string containing the output.
   */
  std::string callLscpu() {
    FILE* pipe = popen("lscpu", "r");
    if (!pipe) {
      return nlohmann::json::object({{"error", "CPU information not available"}});
    }
    char buffer[128];
    std::string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
      result += buffer;
    }
    pclose(pipe);
    return result;
  }

  /**
   * @brief Retrieves CPU information using the `lscpu` command.
   *
   * @return nlohmann::json A JSON object containing CPU information.
   */
  nlohmann::json getCPUInfo() {
    std::string result = callLscpu();
    nlohmann::json cpu_info;
    std::istringstream iss(result);
    std::string line;
    while (std::getline(iss, line)) {
      std::istringstream line_stream(line);
      std::string key, value;
      if (std::getline(line_stream, key, ':') && std::getline(line_stream, value)) {
        cpu_info[key] = trim(value);
      }
    }
    return cpu_info;
  }

  /**
   * @brief Retrieves CPU information using the `lscpu` command.
   *
   * @return nlohmann::json A JSON object containing CPU information.
   */
  nlohmann::json getGPUInfo() {
#ifdef ALPAKA_ACC_GPU_CUDA_ENABLED
    cudaDeviceProp devProp;
    cudaGetDeviceProperties(&devProp, 0);

    return {
        {"name", devProp.name},
        {"totalGlobalMem", devProp.totalGlobalMem},
        {"sharedMemPerBlock", devProp.sharedMemPerBlock},
        {"regsPerBlock", devProp.regsPerBlock},
        {"warpSize", devProp.warpSize},
        {"maxThreadsPerBlock", devProp.maxThreadsPerBlock},
        {"maxThreadsDim",
         {devProp.maxThreadsDim[0], devProp.maxThreadsDim[1], devProp.maxThreadsDim[2]}},
        {"maxGridSize[3]:",
         {devProp.maxGridSize[0], devProp.maxGridSize[1], devProp.maxGridSize[2]}},
        {"clockRate", devProp.clockRate},
        {"totalConstMem", devProp.totalConstMem},
        {"major", devProp.major},
        {"minor", devProp.minor},
        {"multiProcessorCount", devProp.multiProcessorCount},
        {"integrated", devProp.integrated},
        {"canMapHostMemory", devProp.canMapHostMemory},
        {"computeMode", devProp.computeMode},
        {"concurrentKernels", devProp.concurrentKernels},
        {"pciBusID", devProp.pciBusID},
        {"pciDeviceID", devProp.pciDeviceID},
        {"pciDomainID", devProp.pciDomainID},
        {"memoryClockRate", devProp.memoryClockRate},
        {"memoryBusWidth", devProp.memoryBusWidth},
        {"l2CacheSize", devProp.l2CacheSize},
        {"maxThreadsPerMultiProcessor", devProp.maxThreadsPerMultiProcessor},
        {"isMultiGpuBoard", devProp.isMultiGpuBoard},
        {"memPitch", devProp.memPitch},
        {"textureAlignment", devProp.textureAlignment},
        {"texturePitchAlignment", devProp.texturePitchAlignment},
        {"kernelExecTimeoutEnabled", devProp.kernelExecTimeoutEnabled},
        {"unifiedAddressing", devProp.unifiedAddressing},
        {"multiGpuBoardGroupID", devProp.multiGpuBoardGroupID},
        {"singleToDoublePrecisionPerfRatio", devProp.singleToDoublePrecisionPerfRatio},
        {"pageableMemoryAccess", devProp.pageableMemoryAccess},
        {"concurrentManagedAccess", devProp.concurrentManagedAccess},
        {"computePreemptionSupported", devProp.computePreemptionSupported},
        {"canUseHostPointerForRegisteredMem", devProp.canUseHostPointerForRegisteredMem},
        {"cooperativeLaunch", devProp.cooperativeLaunch},
        {"cooperativeMultiDeviceLaunch", devProp.cooperativeMultiDeviceLaunch},
        {"maxTexture1D", devProp.maxTexture1D},
        {"maxTexture1DLinear", devProp.maxTexture1DLinear},
        {"maxTexture2D", {devProp.maxTexture2D[0], devProp.maxTexture2D[1]}},
        {"maxTexture2DLinear",
         {devProp.maxTexture2DLinear[0], devProp.maxTexture2DLinear[1],
          devProp.maxTexture2DLinear[2]}},
        {"maxTexture2DGather", {devProp.maxTexture2DGather[0], devProp.maxTexture2DGather[1]}},
        {"maxTexture3D",
         {devProp.maxTexture3D[0], devProp.maxTexture3D[1], devProp.maxTexture3D[2]}},
        {"maxTextureCubemap", devProp.maxTextureCubemap},
        {"maxTexture1DLayered", {devProp.maxTexture1DLayered[0], devProp.maxTexture1DLayered[1]}},
        {"maxTexture2DLayered",
         {devProp.maxTexture2DLayered[0], devProp.maxTexture2DLayered[1],
          devProp.maxTexture2DLayered[2]}},
        {"maxTextureCubemapLayered",
         {devProp.maxTextureCubemapLayered[0], devProp.maxTextureCubemapLayered[1]}},
        {"maxSurface1D", devProp.maxSurface1D},
        {"maxSurface2D", {devProp.maxSurface2D[0], devProp.maxSurface2D[1]}},
        {"maxSurface3D",
         {devProp.maxSurface3D[0], devProp.maxSurface3D[1], devProp.maxSurface3D[2]}},
        {"maxSurface1DLayered", {devProp.maxSurface1DLayered[0], devProp.maxSurface1DLayered[1]}},
        {"maxSurface2DLayered",
         {devProp.maxSurface2DLayered[0], devProp.maxSurface2DLayered[1],
          devProp.maxSurface2DLayered[2]}},
        {"maxSurfaceCubemap", devProp.maxSurfaceCubemap},
        {"maxSurfaceCubemapLayered",
         {devProp.maxSurfaceCubemapLayered[0], devProp.maxSurfaceCubemapLayered[1]}},
        {"surfaceAlignment", devProp.surfaceAlignment},
        {"ECCEnabled", devProp.ECCEnabled},
        {"tccDriver", devProp.tccDriver},
        {"asyncEngineCount", devProp.asyncEngineCount},
        {"streamPrioritiesSupported", devProp.streamPrioritiesSupported},
        {"globalL1CacheSupported", devProp.globalL1CacheSupported},
        {"localL1CacheSupported", devProp.localL1CacheSupported},
        {"sharedMemPerMultiprocessor", devProp.sharedMemPerMultiprocessor},
        {"regsPerMultiprocessor", devProp.regsPerMultiprocessor},
        {"managedMemory", devProp.managedMemory},
    };

#else
    return {};
#endif
  }

  /**
   * @brief Retrieves the hostname of the system.
   *
   * @return std::string The hostname of the system.
   */
  std::string getHostName() {
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    return std::string(hostname);
  }

  nlohmann::json gatherMetadata() {
    auto now = std::chrono::utc_clock::now();
    nlohmann::json metadata{};
    metadata["start time"] = (std::ostringstream{} << std::format("{0:%F}T{0:%R%z}.", now)).str();
    metadata["host name"] = getHostName();
    metadata["host info"] = getCPUInfo();
    metadata["device info"] = getGPUInfo();
    return metadata;
  }
}  // namespace membenchmc
