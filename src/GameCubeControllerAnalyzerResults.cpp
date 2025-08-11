#include "GameCubeControllerAnalyzerResults.h"

#include "GameCubeControllerAnalyzer.h"
#include "GameCubeControllerAnalyzerSettings.h"

#include <AnalyzerHelpers.h>
#include <fstream>
#include <iostream>

GameCubeControllerAnalyzerResults::GameCubeControllerAnalyzerResults( GameCubeControllerAnalyzer* analyzer,
                                                                      GameCubeControllerAnalyzerSettings* settings )
    : AnalyzerResults(), mSettings( settings ), mAnalyzer( analyzer )
{
}

GameCubeControllerAnalyzerResults::~GameCubeControllerAnalyzerResults()
{
}

void GameCubeControllerAnalyzerResults::GenerateBubbleText( U64 frame_index, Channel& channel, DisplayBase display_base )
{
    ClearResultStrings();
    Frame frame = GetFrame( frame_index );

    // Use frame.mType to determine bubble label
    char result_str[ 64 ];
    if( frame.mType == 1 ) // Command byte
    {
        const char* command_name = nullptr;
        switch( frame.mData1 )
        {
        case GameCubeControllerAnalyzer::CMD_ID:
            command_name = "ID";
            break;
        case GameCubeControllerAnalyzer::CMD_STATUS:
            command_name = "Status";
            break;
        case GameCubeControllerAnalyzer::CMD_ORIGIN:
            command_name = "Origin";
            break;
        case GameCubeControllerAnalyzer::CMD_RECALIBRATE:
            command_name = "Recalibrate";
            break;
        case GameCubeControllerAnalyzer::CMD_STATUS_LONG:
            command_name = "Status Long";
            break;
        case GameCubeControllerAnalyzer::CMD_PROBE_DEVICE:
            command_name = "Probe Device";
            break;
        case GameCubeControllerAnalyzer::CMD_FIX_DEVICE:
            command_name = "Fix Device";
            break;
        default:
            command_name = "Unknown";
            break;
        }
        snprintf( result_str, sizeof( result_str ), "Command: %s", command_name );
        AddResultString( result_str );
        AddResultString( command_name ); // Short version
    }
    else // Data byte
    {
        char hex_str[ 16 ];
        AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 8, hex_str, 16 );
        snprintf( result_str, sizeof( result_str ), "Data: %s", hex_str );
        AddResultString( result_str );
        char short_str[ 16 ];
        snprintf( short_str, sizeof( short_str ), "%02X", ( unsigned int )( frame.mData1 & 0xFF ) );
        AddResultString( short_str );
    }
}

void GameCubeControllerAnalyzerResults::GenerateExportFile( const char* file, DisplayBase display_base, U32 export_type_user_id )
{
    std::ofstream file_stream( file, std::ios::out );

    U64 trigger_sample = mAnalyzer->GetTriggerSample();
    U32 sample_rate = mAnalyzer->GetSampleRate();

    file_stream << "Time [s], Value [0, 1]" << std::endl;

    U64 num_frames = GetNumFrames();
    for( U32 i = 0; i < num_frames; i++ )
    {
        Frame frame = GetFrame( i );

        char time_str[ 128 ];
        AnalyzerHelpers::GetTimeString( frame.mStartingSampleInclusive, trigger_sample, sample_rate, time_str, 128 );

        char number_str[ 128 ];
        AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 8, number_str, 128 );

        file_stream << time_str << "," << number_str << std::endl;

        if( UpdateExportProgressAndCheckForCancel( i, num_frames ) == true )
        {
            file_stream.close();
            return;
        }
    }

    file_stream.close();
}

void GameCubeControllerAnalyzerResults::GenerateFrameTabularText( U64 frame_index, DisplayBase display_base )
{
    Frame frame = GetFrame( frame_index );
    ClearTabularText();

    if( frame.mType == 1 ) // Only add row for command byte
    {
        const char* command_name = nullptr;
        switch( frame.mData1 )
        {
        case GameCubeControllerAnalyzer::CMD_ID:
            command_name = "ID";
            break;
        case GameCubeControllerAnalyzer::CMD_STATUS:
            command_name = "Status";
            break;
        case GameCubeControllerAnalyzer::CMD_ORIGIN:
            command_name = "Origin";
            break;
        case GameCubeControllerAnalyzer::CMD_RECALIBRATE:
            command_name = "Recalibrate";
            break;
        case GameCubeControllerAnalyzer::CMD_STATUS_LONG:
            command_name = "Status Long";
            break;
        case GameCubeControllerAnalyzer::CMD_PROBE_DEVICE:
            command_name = "Probe Device";
            break;
        case GameCubeControllerAnalyzer::CMD_FIX_DEVICE:
            command_name = "Fix Device";
            break;
        default:
            command_name = "Unknown";
            break;
        }
    }
}

void GameCubeControllerAnalyzerResults::GeneratePacketTabularText( U64 packet_id, DisplayBase display_base )
{
    // not supported
}

void GameCubeControllerAnalyzerResults::GenerateTransactionTabularText( U64 transaction_id, DisplayBase display_base )
{
    // not supported
}