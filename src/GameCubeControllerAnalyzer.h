#ifndef GAMECUBECONTROLLER_ANALYZER_H
#define GAMECUBECONTROLLER_ANALYZER_H

#include "GameCubeControllerAnalyzerResults.h"
#include "GameCubeControllerSimulationDataGenerator.h"

#include <Analyzer.h>

class GameCubeControllerAnalyzerSettings;
class ANALYZER_EXPORT GameCubeControllerAnalyzer : public Analyzer2
{
  public:
    enum JoyBusCommand
    {
        CMD_RESET = 0xFF,
        CMD_TYPE_AND_STATUS = 0x00,
        CMD_N64_POLL = 0x01,
        CMD_N64_READ_MEM = 0x02,
        CMD_N64_WRITE_MEM = 0x03,
        CMD_GC_POLL = 0x40,
        CMD_GC_READ_ORIGIN = 0x41,
        CMD_GC_CALIBRATE = 0x42,
        CMD_GC_LONG_POLL = 0x43,
        CMD_GC_PROBE_DEVICE = 0x4D,
        CMD_GC_FIX_DEVICE = 0x4E,
    };

    GameCubeControllerAnalyzer();
    virtual ~GameCubeControllerAnalyzer();

    virtual void SetupResults();
    virtual void WorkerThread();

    virtual U32 GenerateSimulationData( U64 newest_sample_requested, U32 sample_rate, SimulationChannelDescriptor** simulation_channels );
    virtual U32 GetMinimumSampleRateHz();

    virtual const char* GetAnalyzerName() const;
    virtual bool NeedsRerun();

  protected: // vars
    std::unique_ptr<GameCubeControllerAnalyzerSettings> mSettings;
    std::unique_ptr<GameCubeControllerAnalyzerResults> mResults;
    AnalyzerChannelData* mGamecube;

    GameCubeControllerSimulationDataGenerator mSimulationDataGenerator;
    bool mSimulationInitilized;

    U32 mSampleRateHz;
    bool mDecodedTransmission = false;
    bool mDecodedReception = false;

    U64 GetPulseWidthNs( U64 start_edge, U64 end_edge );
    void AdvanceToEndOfPacket();
    bool AdvanceToNextBitInPacket();
    void DecodeFrames();
    bool DecodeByte( U8& byte );
    bool DecodeDataBit( bool& bit );
    bool DecodeStopBit();
};

extern "C" ANALYZER_EXPORT const char* __cdecl GetAnalyzerName();
extern "C" ANALYZER_EXPORT Analyzer* __cdecl CreateAnalyzer();
extern "C" ANALYZER_EXPORT void __cdecl DestroyAnalyzer( Analyzer* analyzer );

#endif // GAMECUBECONTROLLER_ANALYZER_H
