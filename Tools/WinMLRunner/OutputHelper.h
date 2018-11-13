#pragma once
#include "Common.h"
#include "CommandLineArgs.h"
#include <fstream>
#include <ctime>
#include <locale>
#include <utility>
#include <codecvt>
#include <iomanip>
#include <direct.h>

using namespace winrt::Windows::AI::MachineLearning;
using namespace Windows::Storage::Streams;

// Stores performance information and handles output to the command line and CSV files.
class OutputHelper
{
public:
    OutputHelper(int num, bool silent)
    {
        m_silent = silent;
        m_CPUWorkingDiff.resize(num, 0.0);
        m_CPUWorkingStart.resize(num, 0.0);
        m_GPUSharedDiff.resize(num, 0.0);
        m_GPUDedicatedDiff.resize(num, 0.0);
        m_GPUSharedStart.resize(num, 0.0);
    }

    void PrintLoadingInfo(const std::wstring& modelPath) const
    {
        if (!m_silent)
        {
            wprintf(L"Loading model (path = %s)...\n", modelPath.c_str());
        }
    }

    void PrintBindingInfo(uint32_t iteration, DeviceType deviceType, InputBindingType inputBindingType, InputDataType inputDataType, DeviceCreationLocation deviceCreationLocation) const
    {
        if (!m_silent)
        {
            printf(
                "Binding (device = %s, iteration = %d, inputBinding = %s, inputDataType = %s, deviceCreationLocation = %s)...",
                TypeHelper::Stringify(deviceType).c_str(),
                iteration,
                TypeHelper::Stringify(inputBindingType).c_str(),
                TypeHelper::Stringify(inputDataType).c_str(),
                TypeHelper::Stringify(deviceCreationLocation).c_str()
            );
        }
    }

    void PrintEvaluatingInfo(uint32_t iteration, DeviceType deviceType, InputBindingType inputBindingType, InputDataType inputDataType, DeviceCreationLocation deviceCreationLocation) const
    {
        if (!m_silent)
        {
            printf(
                "Evaluating (device = %s, iteration = %d, inputBinding = %s, inputDataType = %s, deviceCreationLocation = %s)...",
                TypeHelper::Stringify(deviceType).c_str(),
                iteration,
                TypeHelper::Stringify(inputBindingType).c_str(),
                TypeHelper::Stringify(inputDataType).c_str(),
                TypeHelper::Stringify(deviceCreationLocation).c_str()
            );
        }
    }

    void PrintModelInfo(std::wstring modelPath, LearningModel model) const
    {
        if (!m_silent)
        {
            std::cout << "=================================================================" << std::endl;
            std::wcout << "Name: " << model.Name().c_str() << std::endl;
            std::wcout << "Author: " << model.Author().c_str() << std::endl;
            std::wcout << "Version: " << model.Version() << std::endl;
            std::wcout << "Domain: " << model.Domain().c_str() << std::endl;
            std::wcout << "Description: " << model.Description().c_str() << std::endl;
            std::wcout << "Path: " << modelPath << std::endl;
            std::cout << "Support FP16: " << std::boolalpha << doesModelContainFP16(model) << std::endl;

            std::cout << std::endl;
            //print out information about input of model
            std::cout << "Input Feature Info:" << std::endl;
            for (auto&& inputFeature : model.InputFeatures())
            {
                PrintFeatureDescriptorInfo(inputFeature);
            }
            //print out information about output of model
            std::cout << "Output Feature Info:" << std::endl;
            for (auto&& outputFeature : model.OutputFeatures())
            {
                PrintFeatureDescriptorInfo(outputFeature);
            }
            std::cout << "=================================================================" << std::endl;
            std::cout << std::endl;
        }
    }

    void PrintFeatureDescriptorInfo(const ILearningModelFeatureDescriptor &descriptor) const
    {
        if (!m_silent)
        {
            //IMPORTANT: This learningModelFeatureKind array needs to match the "enum class 
            //LearningModelFeatureKind" idl in Windows.AI.MachineLearning.0.h
            const std::string learningModelFeatureKind[] =
            {
                "Tensor",
                "Sequence",
                "Map",
                "Image",
            };
            std::wstring name(descriptor.Name());
            std::wcout << "Name: " << name << std::endl;
            std::wcout << "Feature Kind: " << FeatureDescriptorToString(descriptor) << std::endl;
            std::cout << std::endl;
        }
    }

    void PrintHardwareInfo() const
    {
        if (!m_silent)
        {
            std::cout << "WinML Runner" << std::endl;

            com_ptr<IDXGIFactory6> factory;
            CreateDXGIFactory1(__uuidof(IDXGIFactory6), factory.put_void());
            com_ptr<IDXGIAdapter> adapter;
            factory->EnumAdapters(0, adapter.put());
            DXGI_ADAPTER_DESC description;
            if (SUCCEEDED(adapter->GetDesc(&description)))
            {
                std::wcout << L"GPU: " << description.Description << std::endl;
                std::cout << std::endl;
            }
        }
    }

    void PrintResults(
        const Profiler<WINML_MODEL_TEST_PERF> &profiler,
        uint32_t numIterations,
        DeviceType deviceType,
        InputBindingType inputBindingType,
        InputDataType inputDataType,
        DeviceCreationLocation deviceCreationLocation
    ) const
    {
        double loadTime = profiler[LOAD_MODEL].GetAverage(CounterType::TIMER);
        double bindTime = profiler[BIND_VALUE].GetAverage(CounterType::TIMER);
        double evalTime = profiler[EVAL_MODEL].GetAverage(CounterType::TIMER);
        double evalMemoryUsage = profiler[EVAL_MODEL].GetAverage(CounterType::WORKING_SET_USAGE);
        double gpuEvalSharedMemoryUsage = profiler[EVAL_MODEL].GetAverage(CounterType::GPU_SHARED_MEM_USAGE);
        double gpuEvalDedicatedMemoryUsage = profiler[EVAL_MODEL].GetAverage(CounterType::GPU_DEDICATED_MEM_USAGE);

        double totalBindTime = std::accumulate(m_clockBindTimes.begin(), m_clockBindTimes.end(), 0.0);
        double clockBindTime = totalBindTime / (double)numIterations;

        double totalEvalTime = std::accumulate(m_clockEvalTimes.begin(), m_clockEvalTimes.end(), 0.0);
        double clockEvalTime = totalEvalTime / (double)numIterations;

        if (!m_silent)
        {
            double totalTime = (isnan(loadTime) ? 0 : loadTime) + bindTime + evalTime;

            std::cout << std::endl;

            printf("Results (device = %s, numIterations = %d, inputBinding = %s, inputDataType = %s, deviceCreationLocation = %s):\n",
                TypeHelper::Stringify(deviceType).c_str(),
                numIterations,
                TypeHelper::Stringify(inputBindingType).c_str(),
                TypeHelper::Stringify(inputDataType).c_str(),
                TypeHelper::Stringify(deviceCreationLocation).c_str()
            );

            std::cout << "  Load: " << (isnan(loadTime) ? "N/A" : std::to_string(loadTime) + " ms") << std::endl;
            std::cout << "  Bind: " << bindTime << " ms" << std::endl;
            std::cout << "  Evaluate: " << evalTime << " ms" << std::endl;
            std::cout << "  Total Time: " << totalTime << " ms" << std::endl;
            std::cout << "  Wall-Clock Load: " << m_clockLoadTime << " ms" << std::endl;
            std::cout << "  Wall-Clock Bind: " << clockBindTime << " ms" << std::endl;
            std::cout << "  Wall-Clock Evaluate: " << clockEvalTime << " ms" << std::endl;
            std::cout << "  Total Wall-Clock Time: " << (m_clockLoadTime + clockBindTime + clockEvalTime) << " ms" << std::endl;
            std::cout << "  Working Set Memory usage (evaluate): " << gpuEvalDedicatedMemoryUsage << " MB" << std::endl;
            std::cout << "  Dedicated Memory Usage (evaluate): " << gpuEvalDedicatedMemoryUsage << " MB" << std::endl;
            std::cout << "  Shared Memory Usage (evaluate): " << gpuEvalSharedMemoryUsage << " MB" << std::endl;

            std::cout << std::endl << std::endl << std::endl;
        }
    }

    static std::wstring FeatureDescriptorToString(const ILearningModelFeatureDescriptor &descriptor)
    {
        //IMPORTANT: This tensorKinds array needs to match the "enum class TensorKind" idl in Windows.AI.MachineLearning.0.h
        const std::wstring tensorKind[] =
        {
            L"Undefined",
            L"Float",
            L"UInt8",
            L"Int8",
            L"UInt16",
            L"Int16",
            L"Int32",
            L"Int64",
            L"String",
            L"Boolean",
            L"Float16",
            L"Double",
            L"UInt32",
            L"UInt64",
            L"Complex64",
            L"Complex128",
        };
        switch (descriptor.Kind())
        {
        case LearningModelFeatureKind::Tensor:
        {
            auto tensorDescriptor = descriptor.as<TensorFeatureDescriptor>();
            return tensorKind[(int)tensorDescriptor.TensorKind()];
        }
        case LearningModelFeatureKind::Image:
        {
            auto imageDescriptor = descriptor.as<ImageFeatureDescriptor>();
            std::wstring str = L"Image (Height: " + std::to_wstring(imageDescriptor.Height()) +
                               L", Width:  " + std::to_wstring(imageDescriptor.Width()) + L")";
            return str;
        }
        case LearningModelFeatureKind::Map:
        {
            auto mapDescriptor = descriptor.as<MapFeatureDescriptor>();
            std::wstring str = L"Map<" + tensorKind[(int)mapDescriptor.KeyKind()] + L",";
            str += FeatureDescriptorToString(mapDescriptor.ValueDescriptor());
            str += L">";
            return str;
        }
        case LearningModelFeatureKind::Sequence:
        {
            auto sequenceDescriptor = descriptor.as<SequenceFeatureDescriptor>();
            std::wstring str = L"List<" + FeatureDescriptorToString(sequenceDescriptor.ElementDescriptor()) + L">";
            return str;
        }
        default:
            return (L"Invalid feature %s.", descriptor.Name().c_str());
        }
    }

    static bool doesDescriptorContainFP16(const ILearningModelFeatureDescriptor &descriptor)
    {
        switch (descriptor.Kind())
        {
            case LearningModelFeatureKind::Tensor:
            {
                return descriptor.as<TensorFeatureDescriptor>().TensorKind() == TensorKind::Float16;
            }
            break;
            case LearningModelFeatureKind::Map:
            {
                auto mapDescriptor = descriptor.as<MapFeatureDescriptor>();
                if (mapDescriptor.KeyKind() == TensorKind::Float16)
                {
                    return true;
                }
                return doesDescriptorContainFP16(mapDescriptor.ValueDescriptor());
            }
            break;
            case LearningModelFeatureKind::Sequence:
            {
                return doesDescriptorContainFP16(descriptor.as<SequenceFeatureDescriptor>().ElementDescriptor());
            }
            break;
            default:
            {
                return false;
            }
        }
    }

    static bool doesModelContainFP16(const LearningModel model)
    {
        for (auto&& inputFeature : model.InputFeatures())
        {
            if (doesDescriptorContainFP16(inputFeature)) {
                return true;
            }
        }
        return false;
    }

    void SaveEvalTimes(Profiler<WINML_MODEL_TEST_PERF> &profiler, uint32_t IterNum)
    {
        m_CPUWorkingDiff[IterNum] = profiler[EVAL_MODEL].GetCpuWorkingDiff();
        m_CPUWorkingStart[IterNum] = profiler[EVAL_MODEL].GetCpuWorkingStart();
        m_GPUSharedDiff[IterNum] = profiler[EVAL_MODEL].GetGpuSharedDiff();
        m_GPUSharedStart[IterNum] = profiler[EVAL_MODEL].GetGpuSharedStart();
        m_GPUDedicatedDiff[IterNum] = profiler[EVAL_MODEL].GetGpuDedicatedDiff();
    }
    
    void SaveResult(std::vector<std::string> &IterRes, std::vector <int> &TensorHash) 
    {
        m_Result = IterRes;
        m_Hash = TensorHash;
    }
    
    void SetDefaultFolder()
    {
        auto time = std::time(nullptr);
        struct tm localTime;
        localtime_s(&localTime, &time);
        std::string cur_dir = _getcwd(NULL, 0);
        std::ostringstream oss;
        oss << std::put_time(&localTime, "%Y-%m-%d_%H.%M.%S");
        std::string folderName = "\\Run[" + oss.str() + "]";
        folder = cur_dir + folderName;
        if (_mkdir(folder.c_str()) != 0)
            std::cout << "Folder cannot be created";
    }
    
    void SetDefaultCSVFileNamePerIteration()
    {
        fileNameIter = folder + "\\PerIterationValues.csv";
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        m_csvFileNamePerIteration = converter.from_bytes(fileNameIter);
    }
    
    void SetDefaultCSVResult()
    {
        fileNameRes = folder + "\\Result[FullOutputTensor].csv";
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        m_csvResult = converter.from_bytes(fileNameRes);
    }

    void SetDefaultCSVFileName() 
    {
        auto time = std::time(nullptr);
        struct tm localTime;
        localtime_s(&localTime, &time);

        std::ostringstream oss;
        oss << std::put_time(&localTime, "%Y-%m-%d %H.%M.%S");
        std::string fileName = "WinML Runner [" + oss.str() + "].csv";
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        m_csvFileName = converter.from_bytes(fileName);
    }

    void SetCSVFileName(const std::wstring& fileName)
    {
        m_csvFileName = fileName;
    }

    void WritePerformanceDataToCSV(
        const Profiler<WINML_MODEL_TEST_PERF> &profiler,
        int numIterations, std::wstring model,
        std::string modelBinding,
        std::string inputBinding,
        std::string inputType,
        std::string deviceCreationLocation,
        bool firstRunIgnored
    ) const
    {
        double loadTime = profiler[LOAD_MODEL].GetAverage(CounterType::TIMER);
        double bindTime = profiler[BIND_VALUE].GetAverage(CounterType::TIMER);
        double evalTime = profiler[EVAL_MODEL].GetAverage(CounterType::TIMER);
        double evalMemoryUsage = profiler[EVAL_MODEL].GetAverage(CounterType::WORKING_SET_USAGE);
        double gpuEvalSharedMemoryUsage = profiler[EVAL_MODEL].GetAverage(CounterType::GPU_SHARED_MEM_USAGE);
        double gpuEvalDedicatedMemoryUsage = profiler[EVAL_MODEL].GetAverage(CounterType::GPU_DEDICATED_MEM_USAGE);

        double totalBindTime = std::accumulate(m_clockBindTimes.begin(), m_clockBindTimes.end(), 0.0);
        double clockBindTime = totalBindTime / (double)numIterations;

        double totalEvalTime = std::accumulate(m_clockEvalTimes.begin(), m_clockEvalTimes.end(), 0.0);
        double clockEvalTime = totalEvalTime / (double)numIterations;

        double totalTime = (isnan(loadTime) ? 0 : loadTime) + bindTime + evalTime;

        if (!m_csvFileName.empty())
        {
            // Check if header exists
            bool bNewFile = false;
            std::ifstream fin;
            fin.open(m_csvFileName);
            std::filebuf* outbuf = fin.rdbuf();
            if (EOF == outbuf->sbumpc())
            {
                bNewFile = true;
            }
            fin.close();

            std::ofstream fout;
            fout.open(m_csvFileName, std::ios_base::app);

            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
            std::string modelName = converter.to_bytes(model);
       
            if (bNewFile)
            {
                fout << "Model Name" << ","
                     << "Model Binding" << ","
                     << "Input Binding" << ","
                     << "Input Type" << ","
                     << "Device Creation Location" << ","
                     << "Iterations" << ","
                     << "First Run Ignored" << ","
                     << "Load (ms)" << ","
                     << "Bind (ms)" << ","
                     << "Evaluate (ms)" << ","
                     << "Total Time (ms)" << ","
                     << "Working Set Memory usage (evaluate) (MB)" << ","
                     << "GPU Dedicated memory usage (evaluate) (MB)" << ","
                     << "GPU Shared memory usage (evaluate) (MB)" << ","
                     << "Wall-clock Load (ms)" << ","
                     << "Wall-clock Bind (ms)" << ","
                     << "Wall-clock Evaluate (ms)" << ","
                     << "Wall-clock total time (ms)" << ","
                     << "PerIterationFile" << ","
                     << "ResultFile" <<std::endl;
            }

            fout << modelName << ","
                 << modelBinding << ","
                 << inputBinding << ","
                 << inputType << ","
                 << deviceCreationLocation << ","
                 << numIterations << ","
                 << firstRunIgnored << ","
                 << (isnan(loadTime) ? "N/A" : std::to_string(loadTime)) << ","
                 << bindTime << ","
                 << evalTime << ","
                 << totalTime << ","
                 << evalMemoryUsage << ","
                 << gpuEvalDedicatedMemoryUsage << ","
                 << gpuEvalSharedMemoryUsage << ","
                 << m_clockLoadTime << ","
                 << clockBindTime << ","
                 << clockEvalTime << ","
                 << m_clockLoadTime + clockBindTime + clockEvalTime << "," 
                 << fileNameIter << ","
                 << fileNameRes << std::endl;

            fout.close();
        }
    }

    template<typename T>
    void WriteTensorResultToCSV(winrt::Windows::Foundation::Collections::IVectorView<T> &m_Res, int IterNo)
    {
        if (m_csvResult.length() > 0)
        {
            bool bNewFile = false;

            std::ifstream fin;
            fin.open(m_csvResult);
            std::filebuf* outbuf = fin.rdbuf();
            if (EOF == outbuf->sbumpc())
            {
                bNewFile = true;
            }
            fin.close();

            std::ofstream fout;
            fout.open(m_csvResult, std::ios_base::app);

            if (bNewFile)
            {
                fout << "IterationNumber " << ",";
                for (int i = 0; i < m_Res.Size(); i++)
                    fout << "Result[" << i << "]" << ",";
                fout << std::endl;
            }

            fout << IterNo << ",";
            for (int i = 0; i < m_Res.Size(); i++)
                fout << m_Res.GetAt(i) << ",";
            fout << std::endl;
            fout.close();
        }
    }

    template<>
    void WriteTensorResultToCSV(winrt::Windows::Foundation::Collections::IVectorView<winrt::hstring> &m_Res, int IterNo)
    {
        if (m_csvResult.length() > 0)
        {
            bool bNewFile = false;

            std::ifstream fin;
            fin.open(m_csvResult);
            std::filebuf* outbuf = fin.rdbuf();
            if (EOF == outbuf->sbumpc())
            {
                bNewFile = true;
            }
            fin.close();

            std::ofstream fout;
            fout.open(m_csvResult, std::ios_base::app);

            if (bNewFile)
            {
                fout << "IterationNumber " << "," << "Result[0]" << ",";
                fout << std::endl;
            }
            fout << IterNo << "," << m_Res.GetAt(0).data() << std::endl;
            fout.close();
        }
    }

    void WriteSequenceResultToCSV(winrt::Windows::Foundation::Collections::IMap<int64_t, float> &m_Map, int IterNo)
    {
        if (m_csvResult.length() > 0)
        {
            bool bNewFile = false;

            std::ifstream fin;
            fin.open(m_csvResult);
            std::filebuf* outbuf = fin.rdbuf();
            if (EOF == outbuf->sbumpc())
            {
                bNewFile = true;
            }
            fin.close();

            std::ofstream fout;
            fout.open(m_csvResult, std::ios_base::app);

            auto iter = m_Map.First();
            if (bNewFile)
            {
                fout << "IterationNumber " << ",";
                while (iter.HasCurrent())
                {
                    auto pair = iter.Current();
                    fout << "Key[" << pair.Key() << "]" << ",";
                    iter.MoveNext();
                }
                fout << std::endl;
            }
            iter = m_Map.First();
            fout << IterNo << ",";
            while (iter.HasCurrent())
            {
                auto pair = iter.Current();
                fout << pair.Key() << ";" << pair.Value() << ",";
                iter.MoveNext();
            }
            fout << std::endl;
            fout.close();
        }
    }

    void WritePerformanceDataToCSVPerIteration(Profiler<WINML_MODEL_TEST_PERF> &profiler, const CommandLineArgs& args, std::wstring model, std::wstring img)
    {
        if (m_csvFileNamePerIteration.length() > 0)
        {
            bool bNewFile = false;
            std::ifstream fin;
            fin.open(m_csvFileNamePerIteration);
            std::filebuf* outbuf = fin.rdbuf();
            if (EOF == outbuf->sbumpc())
            {
                bNewFile = true;
            }
            fin.close();
            
            std::ofstream fout;
            fout.open(m_csvFileNamePerIteration, std::ios_base::app);
            
            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
            std::string modelName = converter.to_bytes(model);
            std::string imgName = converter.to_bytes(img);
            
            if (bNewFile)
            {
                fout << "Model Name" << ","
                     << "Image Name" << ","
                     << "Iterations" << ","
                     << "Iteration Number " << ","
                     << "Result" << ","
                     << "Hash" << ","
                     << "CPU Working Set Diff" << ","
                     << "CPU Working Set Start (MB)" << ","
                     << "GPU Shared Memory Diff (MB)" << ","
                     << "GPU Shared Memory Start (MB)" << ","
                     << "GPU Dedicated Memory Diff (MB)" << ","
                     << "Load (ms)" << ","
                     << "Bind (ms)" << ","
                     << "Evaluate (ms)" << "," << std::endl;
            }

            m_clockLoadTimes.resize(args.NumIterations(), 0.0);

            for (int i = 0; i < args.NumIterations(); i++) 
            {
                fout << modelName << ","
                     << imgName << ","
                     << args.NumIterations() << ","
                     << i + 1 << ","
                     << m_Result[i] << ","
                     << m_Hash[i] << ","
                     << m_CPUWorkingDiff[i] << ","
                     << m_CPUWorkingStart[i] << ","
                     << m_GPUSharedDiff[i] << ","
                     << m_GPUSharedStart[i] << ","
                     << m_GPUDedicatedDiff[i] << ","
                     << m_clockLoadTimes[i] << ","
                     << m_clockBindTimes[i] << ","
                     << m_clockEvalTimes[i] << std::endl;
            }
            fout.close();
        }
    }
    
    void ResetBindAndEvalTImes() 
    {
         m_clockEvalTime = 0;
         m_clockBindTime = 0;

         m_clockBindTimes.clear();
         m_clockEvalTimes.clear();
    }

    void ResetMemoryAndResult()
    {
        m_CPUWorkingDiff.clear();
        m_CPUWorkingStart.clear();
        m_GPUSharedDiff.clear();
        m_GPUDedicatedDiff.clear();
        m_GPUSharedStart.clear();
        m_Result.clear();
        m_Hash.clear();
    }

    double m_clockLoadTime = 0;

    std::vector<double> m_clockLoadTimes;
    std::vector<double> m_clockBindTimes;
    std::vector<double> m_clockEvalTimes;

private:
    std::wstring m_csvFileName;
    std::wstring m_csvFileNamePerIteration;
    std::wstring m_csvResult;
    std::string folder;
    std::string fileNameIter;
    std::string fileNameRes;

    double m_clockBindTime = 0;
    double m_clockEvalTime = 0;

    bool m_silent = false;

    std::vector<double> m_CPUWorkingDiff;
    std::vector<double> m_CPUWorkingStart;
    std::vector<double> m_GPUSharedDiff;
    std::vector<double> m_GPUSharedStart;
    std::vector<double> m_GPUDedicatedDiff;
    std::vector<std::string> m_Result;
    std::vector<int> m_Hash;
};