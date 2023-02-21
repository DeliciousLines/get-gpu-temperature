///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This is a simple example that shows you how to get GPU temperature. The main code lies in two sub-programs: get_temperature_for_nvidia()
// and get_temperature_for_amd().
// This particular example assumes it runs on Windows.
//
// To compile this simply give entry_point.c to your compiler of choice.
// MSVC example: cl /O2 /TC entry_point.c /Fegpu_temperature.exe
//
// For more information on the APIs used, check the following links:
//     For NVIDIA: https://developer.nvidia.com/rtx/path-tracing/nvapi/get-started
//     For AMD:    https://gpuopen.com/adl/
//
// This file is placed under the MIT license.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define _CRT_SECURE_NO_WARNINGS // Otherwise Windows goes nuts.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// This is necessary to load the ADL DLL and its procedures.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#pragma comment(lib, "kernel32")
////////////////////////////////////////////////////////////

#pragma comment(lib, "nvapi64")
#include "nvapi_headers/nvapi.h"

#include "adl_headers/adl_sdk.h"


// Very simple memory allocator.
void* global_memory        = NULL; // This is initialised in main().
int   global_memory_offset = 0;

void* allocate_memory(int size)
{
    void* data = global_memory + global_memory_offset;
    global_memory_offset += size;
    
    return data;
}
////////////////////////////////



char* get_nvapi_status_string(NvAPI_Status status)
{
    NvAPI_ShortString status_string = {};
    NvAPI_GetErrorMessage(status, status_string);
    
    char* result = allocate_memory(sizeof(NvAPI_ShortString));
    
    int result_size = strlen(status_string);
    if(!result_size) sprintf(result, "0x%x", status);
    else memcpy(result, status_string, result_size);
    
    return result;
}

int get_temperature_for_nvidia()
{
    int max_temperature = 0;

    // Initialise the API. START
    NvAPI_Status status = NvAPI_Initialize();
    if(status != NVAPI_OK)
    {
        printf("\nFailed to initialise NvAPI with error %s.", get_nvapi_status_string(status));
        return 0;
    }
    // Initialise the API. END
    
    
    // Enumerate GPUs. START
    NvPhysicalGpuHandle gpus[NVAPI_MAX_PHYSICAL_GPUS] = {};
    
    NvU32 num_gpus = 0;
    status = NvAPI_EnumPhysicalGPUs(gpus, &num_gpus);
    if(status != NVAPI_OK)
    {
        printf("\nFailed to enumerate NVIDIA GPUs with error %s.", get_nvapi_status_string(status));
        goto nvapi_deinit;
    }
    // Enumerate GPUs. END
    
    
    // Get temperature. START
    NV_GPU_THERMAL_SETTINGS_V2 info;
    
    for(int gpu_index = 0; gpu_index < num_gpus; gpu_index++)
    {
        NvPhysicalGpuHandle gpu_handle = gpus[gpu_index];
        
        info.version = NV_GPU_THERMAL_SETTINGS_VER_2;
        
        status = NvAPI_GPU_GetThermalSettings(gpu_handle, NVAPI_THERMAL_TARGET_ALL, &info);
        if(status != NVAPI_OK) continue;
        
        
        for(int sensor_index = 0; sensor_index < info.count; sensor_index++)
        {
            int temperature = info.sensor[sensor_index].currentTemp;
            if(temperature > max_temperature) max_temperature = temperature;
        }
    }
    // Get temperature. END
    
    
    // Deinitialise the API.
    nvapi_deinit:;
    NvAPI_Unload();
    ////////////////////////

    return max_temperature;
}


int get_temperature_for_amd()
{
    int max_temperature = 0;

    // Initialise ADL. START
    typedef int   _ADL2_Main_Control_Create(ADL_MAIN_MALLOC_CALLBACK callback, int iEnumConnectedAdapters, ADL_CONTEXT_HANDLE* context);
    typedef int   _ADL2_Main_Control_Destroy(ADL_CONTEXT_HANDLE context);
    typedef void* _ADL2_Main_Control_GetProcAddress(ADL_CONTEXT_HANDLE context, void* lpModule, char* lpProcName);
    typedef int   _ADL2_Adapter_AdapterInfoX3_Get(ADL_CONTEXT_HANDLE context, int iAdapterIndex, int* numAdapters, AdapterInfo** lppAdapterInfo);
    typedef int   _ADL2_Overdrive5_Temperature_Get(ADL_CONTEXT_HANDLE context, int iAdapterIndex, int iThermalControllerIndex, ADLTemperature* lpTemperature);
    
    _ADL2_Main_Control_Create*         ADL2_Main_Control_Create         = NULL;
    _ADL2_Main_Control_Destroy*        ADL2_Main_Control_Destroy        = NULL;
    _ADL2_Main_Control_GetProcAddress* ADL2_Main_Control_GetProcAddress = NULL;
    _ADL2_Adapter_AdapterInfoX3_Get*   ADL2_Adapter_AdapterInfoX3_Get   = NULL;
    _ADL2_Overdrive5_Temperature_Get*  ADL2_Overdrive5_Temperature_Get  = NULL;
    
    void* adl_library = LoadLibraryA("atiadlxx.dll");
    if(!adl_library)
    {
        printf("\nFailed to load ADL DLL.");
        return 0;
    }
    
    ADL2_Main_Control_Create         = (_ADL2_Main_Control_Create*) GetProcAddress(adl_library, "ADL2_Main_Control_Create");
    ADL2_Main_Control_Destroy        = (_ADL2_Main_Control_Destroy*) GetProcAddress(adl_library, "ADL2_Main_Control_Destroy");
    ADL2_Main_Control_GetProcAddress = (_ADL2_Main_Control_GetProcAddress*) GetProcAddress(adl_library, "ADL2_Main_Control_GetProcAddress");
    
    ADL_CONTEXT_HANDLE adl_context = {0};
    int status = ADL2_Main_Control_Create(allocate_memory, 1, &adl_context);
    if(status != ADL_OK)
    {
        printf("\nFailed to make an ADL context with status 0x%x.", status);
        goto adl_deinit;
    }
    
    ADL2_Overdrive5_Temperature_Get = ADL2_Main_Control_GetProcAddress(adl_context, adl_library, "ADL2_Overdrive5_Temperature_Get");
    ADL2_Adapter_AdapterInfoX3_Get  = ADL2_Main_Control_GetProcAddress(adl_context, adl_library, "ADL2_Adapter_AdapterInfoX3_Get");
    // Initialise ADL. END
    
    
    // Enumerate GPUs. START
    int num_adapters = 0;
    AdapterInfo* adapters = NULL;
    status = ADL2_Adapter_AdapterInfoX3_Get(adl_context, -1, &num_adapters, &adapters);
    if(status != ADL_OK)
    {
        printf("\n Failed to enumerate AMD GPUs.");
        goto adl_deinit;
    }
    // Enumerate GPUs. END
    
    
    // Get temperature. START
    for(int adapter_info_index = 0; adapter_info_index < num_adapters; adapter_info_index++)
    {
        int adapter_index = adapters[adapter_info_index].iAdapterIndex;
        
        ADLTemperature adl_temperature = {.iSize = sizeof(ADLTemperature)};
        status = ADL2_Overdrive5_Temperature_Get(adl_context, adapter_index, 0, &adl_temperature);
        if(status != ADL_OK) continue;
        
        
        int temperature = adl_temperature.iTemperature / 1000; // ADL returns millidegrees Celsius.
        if(temperature > max_temperature) max_temperature = temperature;
    }
    // Get temperature. END
    
    
    // Deinitialise ADL.
    adl_deinit:;
    ADL2_Main_Control_Destroy(adl_context);
    FreeLibrary(adl_library);
    ////////////////////
    

    return max_temperature;
}


int main()
{
    global_memory = malloc(1024 * 1024 * 8); // Allocate 8 MB.

    int nvidia_temperature = get_temperature_for_nvidia();
    int amd_temperature    = get_temperature_for_amd();
    
    printf(
        "\n\n"
        "NVIDIA temperature: %d C\n"
        "AMD temperature:    %d C\n",
        nvidia_temperature, amd_temperature
    );

    return 0;
}