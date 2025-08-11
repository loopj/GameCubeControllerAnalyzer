#include "GameCubeControllerAnalyzer.h"

#include "GameCubeControllerAnalyzerSettings.h"

#include <AnalyzerChannelData.h>

GameCubeControllerAnalyzer::GameCubeControllerAnalyzer()
    : Analyzer2(), mSettings( new GameCubeControllerAnalyzerSettings() ), mSimulationInitilized( false )
{
    SetAnalyzerSettings( mSettings.get() );
    UseFrameV2();
}

GameCubeControllerAnalyzer::~GameCubeControllerAnalyzer()
{
    KillThread();
}

void GameCubeControllerAnalyzer::SetupResults()
{
    mResults.reset( new GameCubeControllerAnalyzerResults( this, mSettings.get() ) );
    SetAnalyzerResults( mResults.get() );
    mResults->AddChannelBubblesWillAppearOn( mSettings->mInputChannel );
}

void GameCubeControllerAnalyzer::WorkerThread()
{
    mSampleRateHz = GetSampleRate();

    mGamecube = GetAnalyzerChannelData( mSettings->mInputChannel );

    AdvanceToEndOfPacket();

    while( true )
    {
        DecodeFrames();
        ReportProgress( mGamecube->GetSampleNumber() );
        CheckIfThreadShouldExit();
    }
}

bool GameCubeControllerAnalyzer::NeedsRerun()
{
    return false;
}

U32 GameCubeControllerAnalyzer::GenerateSimulationData( U64 minimum_sample_index, U32 device_sample_rate,
                                                        SimulationChannelDescriptor** simulation_channels )
{
    if( mSimulationInitilized == false )
    {
        mSimulationDataGenerator.Initialize( GetSimulationSampleRate(), mSettings.get() );
        mSimulationInitilized = true;
    }

    return mSimulationDataGenerator.GenerateSimulationData( minimum_sample_index, device_sample_rate, simulation_channels );
}

U32 GameCubeControllerAnalyzer::GetMinimumSampleRateHz()
{
    return 2000000;
}

const char* GameCubeControllerAnalyzer::GetAnalyzerName() const
{
    return "GameCube";
}

const char* GetAnalyzerName()
{
    return "GameCube";
}

Analyzer* CreateAnalyzer()
{
    return new GameCubeControllerAnalyzer();
}

void DestroyAnalyzer( Analyzer* analyzer )
{
    delete analyzer;
}

U64 GameCubeControllerAnalyzer::GetPulseWidthNs( U64 start_edge, U64 end_edge )
{
    return static_cast<U64>( ( end_edge - start_edge ) * 1000000000 / mSampleRateHz );
}

// advances to the rising edge at the end of a packet
void GameCubeControllerAnalyzer::AdvanceToEndOfPacket()
{
    if( mGamecube->GetBitState() == BIT_LOW )
    {
        mGamecube->AdvanceToNextEdge();
    }

    // if a complete packet was received successfully, we're already at the end of the packet
    if( mDecodedReception )
    {
        mDecodedReception = false;
        return;
    }

    // otherwise, something was corrupted. synchronize to at least 100us of inactivity.
    // this way, we can be sure we're at the beginning of a transmission and not in between
    // a transmission and reception
    while( GetPulseWidthNs( mGamecube->GetSampleNumber(), mGamecube->GetSampleOfNextEdge() ) < 100000 )
    {
        mGamecube->AdvanceToNextEdge();
        mGamecube->AdvanceToNextEdge();
    }
}

// advances to the falling edge of the next bit in a packet
bool GameCubeControllerAnalyzer::AdvanceToNextBitInPacket()
{
    // if the transmission from the host completed, the controller has ~100us to respond
    // in this condition, provide the extra leniency
    int duration = mDecodedTransmission ? 100000 : 5000;
    mDecodedTransmission = false;

    if( GetPulseWidthNs( mGamecube->GetSampleNumber(), mGamecube->GetSampleOfNextEdge() ) < duration )
    {
        mGamecube->AdvanceToNextEdge();
        return true;
    }

    return false;
}

// SIMPLIFIED DECODER: Just output raw bytes with hex values in bubbles
void GameCubeControllerAnalyzer::DecodeFrames()
{
    // traverse to the first falling edge
    mGamecube->AdvanceToNextEdge();

    // Start by decoding the command byte
    U8 command_byte;
    U64 byte_start_sample = mGamecube->GetSampleNumber();

    if( !DecodeByte( command_byte ) )
    {
        AdvanceToEndOfPacket();
        return;
    }

    // Create frame for command byte
    U64 byte_end_sample = mGamecube->GetSampleOfNextEdge(); // Use next falling edge for end
    Frame frame;
    frame.mStartingSampleInclusive = byte_start_sample;
    frame.mEndingSampleInclusive = byte_end_sample - 1; // End just before next byte or stop bit starts
    frame.mType = 1;                                    // Command byte
    frame.mData1 = command_byte;
    mResults->AddFrame( frame );
// AddFrameV2 for Data Table support
#ifdef LOGIC2
    FrameV2 framev2;
    switch( command_byte )
    {
    case CMD_ID:
        framev2.AddString( "Command", "ID" );
        break;
    case CMD_STATUS:
        framev2.AddString( "Command", "Status" );
        break;
    case CMD_ORIGIN:
        framev2.AddString( "Command", "Origin" );
        break;
    case CMD_RECALIBRATE:
        framev2.AddString( "Command", "Recalibrate" );
        break;
    case CMD_STATUS_LONG:
        framev2.AddString( "Command", "Status Long" );
        break;
    case CMD_PROBE_DEVICE:
        framev2.AddString( "Command", "Probe Device" );
        break;
    case CMD_FIX_DEVICE:
        framev2.AddString( "Command", "Fix Device" );
        break;
    default:
        framev2.AddString( "Command", "Unknown" );
        break;
    }
    framev2.AddByte( "Value", command_byte );
    mResults->AddFrameV2( framev2, "Command", byte_start_sample, byte_end_sample - 1 );
#endif
    mResults->CommitResults();

    // Determine command structure based on command byte
    int command_length = 1;  // Default: just the command byte
    int response_length = 0; // Default: no response

    switch( command_byte )
    {
    case CMD_ID:             // 0x00
        command_length = 1;  // Just command byte
        response_length = 3; // 2 bytes device ID + 1 byte status
        break;

    case CMD_STATUS:         // 0x40
        command_length = 3;  // Command + 2 argument bytes
        response_length = 8; // Controller state data
        break;

    case CMD_ORIGIN:          // 0x41
        command_length = 1;   // Just command byte
        response_length = 10; // Origin calibration data
        break;

    case CMD_RECALIBRATE:     // 0x42
        command_length = 3;   // Command + 2 argument bytes
        response_length = 10; // Recalibration response data
        break;

    case CMD_STATUS_LONG:     // 0x43
        command_length = 3;   // Command + 2 argument bytes
        response_length = 10; // Long status response data
        break;

    case CMD_PROBE_DEVICE:   // 0x4D
        command_length = 3;  // Command + 2 argument bytes
        response_length = 8; // Probe response data
        break;

    case CMD_FIX_DEVICE:     // 0x4E
        command_length = 3;  // Command + 2 argument bytes
        response_length = 3; // Fix device response
        break;

    default:
        // Unknown command, just decode what we can
        command_length = 1;
        response_length = 0;
        break;
    }

    // Decode remaining command bytes (if any)
    for( int i = 1; i < command_length; i++ )
    {
        if( !AdvanceToNextBitInPacket() )
            break;

        byte_start_sample = mGamecube->GetSampleNumber();
        U8 byte;

        if( !DecodeByte( byte ) )
            break;

        byte_end_sample = mGamecube->GetSampleOfNextEdge(); // Use next falling edge for end
        Frame frame;
        frame.mStartingSampleInclusive = byte_start_sample;
        frame.mEndingSampleInclusive = byte_end_sample - 1;
        frame.mType = 0; // Data byte
        frame.mData1 = byte;
        mResults->AddFrame( frame );
        mResults->CommitResults();
    }

    // Look for stop bit if we expect a response
    if( response_length > 0 )
    {
        if( AdvanceToNextBitInPacket() && DecodeStopBit() )
        {
            mDecodedTransmission = true;

            // Decode response bytes
            for( int i = 0; i < response_length; i++ )
            {
                if( !AdvanceToNextBitInPacket() )
                    break;

                byte_start_sample = mGamecube->GetSampleNumber();
                U8 byte;

                if( !DecodeByte( byte ) )
                    break;

                byte_end_sample = mGamecube->GetSampleOfNextEdge(); // Use next falling edge for end
                Frame frame;
                frame.mStartingSampleInclusive = byte_start_sample;
                frame.mEndingSampleInclusive = byte_end_sample - 1;
                frame.mType = 0; // Data byte
                frame.mData1 = byte;
                mResults->AddFrame( frame );
                mResults->CommitResults();
            }

            // Look for final stop bit
            if( AdvanceToNextBitInPacket() )
            {
                DecodeStopBit();
                mDecodedReception = true;
            }
        }
    }
    else
    {
        // No response expected, just look for stop bit to end command
        if( AdvanceToNextBitInPacket() )
        {
            DecodeStopBit();
            mDecodedTransmission = true;
        }
    }

    AdvanceToEndOfPacket();
}

// attempts to decode a byte. the current sample should be a falling edge and this
// function will return on a rising edge
bool GameCubeControllerAnalyzer::DecodeByte( U8& byte )
{
    byte = 0;
    for( U8 i = 0; i < 8; i++ )
    {
        bool bit;
        if( !DecodeDataBit( bit ) )
        {
            return false;
        }

        byte |= bit << ( 7 - i );

        if( i < 7 )
        {
            // advance to the next falling edge iff
            // - there are more bits to process in the current byte
            // - the last bit was successful
            mGamecube->AdvanceToNextEdge();
        }
    }

    return true;
}

// attempts to decode a single bit. on entry, the current sample should be a falling edge and this
// function will return on a rising edge
bool GameCubeControllerAnalyzer::DecodeDataBit( bool& bit )
{
    U64 starting_sample, ending_sample, rising_edge_sample, falling_edge_sample;

    // determine whether the bit is a 1 or 0 based on the duration of the low time
    starting_sample = falling_edge_sample = mGamecube->GetSampleNumber();
    mGamecube->AdvanceToNextEdge();
    rising_edge_sample = mGamecube->GetSampleNumber();

    U64 low_time = GetPulseWidthNs( falling_edge_sample, rising_edge_sample );

    if( low_time >= 5000 )
    {
        return false;
    }
    else
    {
        bit = low_time < 2000;

        // make sure the high time is reasonable. peek at the next falling edge, but don't
        // actually advance to it yet, in case something is wrong.
        ending_sample = falling_edge_sample = mGamecube->GetSampleOfNextEdge();
        U64 high_time = GetPulseWidthNs( rising_edge_sample, falling_edge_sample );

        if( high_time >= 5000 )
        {
            return false;
        }

        // add an indicator showing the bit value
        U64 middle_sample = ( starting_sample + ending_sample ) / 2;
        if( bit )
        {
            mResults->AddMarker( middle_sample, AnalyzerResults::One, mSettings->mInputChannel );
        }
        else
        {
            mResults->AddMarker( middle_sample, AnalyzerResults::Zero, mSettings->mInputChannel );
        }
    }

    return true;
}

// attempt to detect a stop bit, which is a single "1" bit where the high time doesn't matter.
// on entry, the current sample should be a falling edge and this function will return on a rising
// edge
bool GameCubeControllerAnalyzer::DecodeStopBit()
{
    U64 falling_edge_sample = mGamecube->GetSampleNumber();
    mGamecube->AdvanceToNextEdge();
    U64 rising_edge_sample = mGamecube->GetSampleNumber();

    U64 low_time = GetPulseWidthNs( falling_edge_sample, rising_edge_sample );

    // after observing an OEM controller, the low-time of a stop bit tended to be more than an
    // average "1" but less than a "0". therefore, we add a bit of leniency.
    bool is_stop_bit = low_time < 2500;

    if( is_stop_bit )
    {
        // Add a stop bit marker at the middle of the bit period
        U64 middle_sample = ( falling_edge_sample + rising_edge_sample ) / 2;
        mResults->AddMarker( middle_sample, AnalyzerResults::Stop, mSettings->mInputChannel );
    }

    return is_stop_bit;
}
