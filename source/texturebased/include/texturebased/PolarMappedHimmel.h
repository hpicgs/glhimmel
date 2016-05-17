#pragma once

#include <memory>
#include <texturebased/texturebased_api.h>
#include <texturebased/AbstractMappedHimmel.h>
#include <texturebased/HorizonBand.h>

#include <globjects/base/ref_ptr.h>


#include <map>

namespace globjects {
    class Shader;
    class Texture;
}


namespace glHimmel
{

    // uses method presented by Blinn, James F. and Newell, Martin E.
    // in "Texture and reflection in computer generated images" 1976

    class TEXTUREBASED_API PolarMappedHimmel : public AbstractMappedHimmel
    {
    public:

        enum class MappingMode
        {
            Full
            , Half
        };

    public:

        PolarMappedHimmel(
            const MappingMode &mappingMode = MappingMode::Half
            , const bool hBand = false
            , const bool fakeSun = false);

        virtual ~PolarMappedHimmel() = default;

        MappingMode getMappingMode() const
        {
            return m_mappingMode;
        }

        void update() override;

    protected:

        std::string fragmentShaderPath() const override;

    protected:

        MappingMode m_mappingMode;

        std::unique_ptr<HorizonBand> m_hBand;
    };

} // namespace glHimmel

