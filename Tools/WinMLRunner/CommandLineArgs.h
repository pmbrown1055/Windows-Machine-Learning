#pragma once
#include "Common.h"

class CommandLineArgs
{
public:
    CommandLineArgs();
    void PrintUsage();

    bool UseCPU() const { return m_useCPU; }
    bool UseGPU() const { return m_useGPU; }
    bool UseCPUandGPU() const { return m_useCPUandGPU; }
    Windows::AI::MachineLearning::LearningModelDeviceKind DeviceKind() const { return m_deviceKind; }
    bool PerfCapture() const { return m_perfCapture; }
    bool EnableDebugOutput() const { return m_debug;  }
  bool PerIterCapture() const { return m_perIterCapture; }
   
    const std::wstring& ImagePath() const { return m_imagePath; }
    const std::wstring& CsvPath() const { return m_csvData; }

    const float Scale() const { return m_scale; }
    const std::array<float, 3>& MeanStdDev() const { return m_meanStdDev; }

    const std::wstring& FolderPath() const { return m_modelFolderPath; }
    const std::wstring& ModelPath() const { return m_modelPath; }
    void SetModelPath(std::wstring path) { m_modelPath = path; }

    UINT NumIterations() const { return m_numIterations; }

private:
    bool m_perfCapture = false;
    bool m_useCPU = false;
    bool m_useGPU = false;
    bool m_useCPUandGPU = false;
    bool m_debug = false;
  bool m_perIterCapture = false;
    Windows::AI::MachineLearning::LearningModelDeviceKind m_deviceKind = Windows::AI::MachineLearning::LearningModelDeviceKind::DirectX;

    std::wstring m_modelFolderPath;
    std::wstring m_modelPath;
    std::wstring m_imagePath;

    float m_scale;
    std::array<float, 3> m_meanStdDev;
    
    std::wstring m_csvData;
    std::wstring m_inputData;
    UINT m_numIterations = 1;
};
