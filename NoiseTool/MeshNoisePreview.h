#pragma once
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <cstring>
#include <thread>

#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Color.h>
#include <Magnum/GL/AbstractShaderProgram.h>
#include <Magnum/GL/Shader.h>

#include "FastNoise/FastNoise.h"
#include "MultiThreadQueues.h"

namespace Magnum
{
    class MeshNoisePreview
    {
    public:
        MeshNoisePreview();
        ~MeshNoisePreview();

        void ReGenerate( FastNoise::SmartNodeArg<> generator );

        void Draw( const Matrix4& transformation, const Matrix4& projection, const Vector3& cameraPosition );

    private:
        class VertexLightShader : public GL::AbstractShaderProgram
        {
        public:
            typedef GL::Attribute<0, Vector4> PositionLight;

            explicit VertexLightShader();
            explicit VertexLightShader( NoCreateT ) noexcept : AbstractShaderProgram{ NoCreate } {}

            VertexLightShader( const VertexLightShader& ) = delete;
            VertexLightShader( VertexLightShader&& ) noexcept = default;
            VertexLightShader& operator=( const VertexLightShader& ) = delete;
            VertexLightShader& operator=( VertexLightShader&& ) noexcept = default;

            VertexLightShader& SetTransformationProjectionMatrix( const Matrix4& matrix );
            VertexLightShader& SetColorTint( const Color3& color );

        private:
            GL::Shader CreateShader( GL::Version version, GL::Shader::Type type );

            int mTransformationProjectionMatrixUniform = 0;
            int mColorTintUniform = 1;
        };

        class Chunk
        {
        public:
            struct VertexData
            {
                VertexData( Vector3 p, float c ) :
                    posLight( p, c ) {}

                Vector4 posLight;
            };

            struct MeshData
            {
                MeshData() = default;

                MeshData( Vector3i p, FastNoise::OutputMinMax mm, const std::vector<VertexData>& v, const std::vector<uint32_t>& i ) : pos( p ), minMax( mm )
                {
                    if( v.empty() )
                    {
                        return;
                    }

                    size_t vertSize = sizeof( VertexData ) * v.size();
                    size_t indSize = sizeof( uint32_t ) * i.size();

                    void* vertexDataPtr = std::malloc( vertSize + indSize );
                    void* indiciesPtr = (uint8_t*)vertexDataPtr + vertSize;

                    std::memcpy( vertexDataPtr, v.data(), vertSize );
                    std::memcpy( indiciesPtr, i.data(), indSize );

                    vertexData = { (VertexData*)vertexDataPtr, v.size() };
                    indicies = { (uint32_t*)indiciesPtr, i.size() };
                }

                void Free()
                {
                    std::free( vertexData.data() );

                    vertexData = nullptr;
                    indicies = nullptr;
                }

                Vector3i pos;
                Containers::ArrayView<VertexData> vertexData;
                Containers::ArrayView<uint32_t> indicies;
                FastNoise::OutputMinMax minMax;
            };

            struct BuildData
            {
                FastNoise::SmartNode<const FastNoise::Generator> generator;
                Vector3i pos;
                Color3 color;
                float frequency, isoSurface;
                int32_t seed;
                uint32_t genVersion;
            };

            static MeshData BuildMeshData( const BuildData& buildData );

            Chunk( MeshData& meshData );

            GL::Mesh& GetMesh() { return mMesh; }
            Vector3i GetPos() const { return mPos; }

            static constexpr uint32_t SIZE          = 128;
            static constexpr Vector3  LIGHT_DIR     = { 3, 4, 2 };
            static constexpr float    AMBIENT_LIGHT = 0.3f;
            static constexpr float    AO_STRENGTH   = 0.6f;

        private:
            static void AddQuadAO( std::vector<VertexData>& verts, std::vector<uint32_t>& indicies, const float* density, float isoSurface,
                                   int32_t idx, int32_t facingIdx, int32_t offsetA, int32_t offsetB, float light, Vector3 pos00, Vector3 pos01, Vector3 pos11, Vector3 pos10 );

            static constexpr uint32_t SIZE_GEN = SIZE + 2;

            Vector3i mPos;
            GL::Mesh mMesh{ NoCreate };
        };

        struct Vector3iHash
        {
            size_t operator()( const Vector3i& v ) const
            {
                return (size_t)v.x() ^ ((size_t)v.y() << sizeof( size_t ) * 2) ^ ((size_t)v.z() << sizeof( size_t ) * 4);
            }
        };

        static void GenerateLoopThread( GenerateQueue<Chunk::BuildData>& generateQueue, CompleteQueue<Chunk::MeshData>& completeQueue );

        void UpdateChunksForPosition( Vector3 position );
        void UpdateChunkQueues( const Vector3& position );
        float GetLoadRangeModifier();

        void StartTimer();
        float GetTimerDurationMs();
        void SetupSettingsHandlers();

        std::unordered_map<Vector3i, Chunk, Vector3iHash> mChunks;
        std::unordered_set<Vector3i, Vector3iHash> mInProgressChunks;
        std::vector<Vector3i> mDistanceOrderedChunks;

        bool mEnabled = true;
        Chunk::BuildData mBuildData;
        float mLoadRange = 300.0f;
        float mAvgNewChunks = 1.0f;
        uint32_t mTriLimit = 35000000; // 35 mil
        uint32_t mTriCount = 0;
        int mStaggerCheck = 0;
        FastNoise::OutputMinMax mMinMax;

        GenerateQueue<Chunk::BuildData> mGenerateQueue;
        CompleteQueue<Chunk::MeshData> mCompleteQueue;
        std::vector<std::thread> mThreads;
        std::chrono::high_resolution_clock::time_point mTimerStart;

        VertexLightShader mShader;
    };
}
