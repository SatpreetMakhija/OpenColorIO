/*
Copyright (c) 2003-2010 Sony Pictures Imageworks Inc., et al.
All Rights Reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
* Neither the name of Sony Pictures Imageworks nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <OpenColorIO/OpenColorIO.h>

#include "GpuAllocationNoOp.h"
#include "OpBuilders.h"


OCIO_NAMESPACE_ENTER
{
    ColorSpaceTransformRcPtr ColorSpaceTransform::Create()
    {
        return ColorSpaceTransformRcPtr(new ColorSpaceTransform(), &deleter);
    }
    
    void ColorSpaceTransform::deleter(ColorSpaceTransform* t)
    {
        delete t;
    }
    
    
    class ColorSpaceTransform::Impl
    {
    public:
        TransformDirection dir_;
        std::string src_;
        std::string dst_;
        
        Impl() :
            dir_(TRANSFORM_DIR_FORWARD)
        { }
        
        ~Impl()
        { }
        
        Impl& operator= (const Impl & rhs)
        {
            dir_ = rhs.dir_;
            src_ = rhs.src_;
            dst_ = rhs.dst_;
            return *this;
        }
    };
    
    ///////////////////////////////////////////////////////////////////////////
    
    
    
    ColorSpaceTransform::ColorSpaceTransform()
        : m_impl(new ColorSpaceTransform::Impl)
    {
    }
    
    TransformRcPtr ColorSpaceTransform::createEditableCopy() const
    {
        ColorSpaceTransformRcPtr transform = ColorSpaceTransform::Create();
        *(transform->m_impl) = *m_impl;
        return transform;
    }
    
    ColorSpaceTransform::~ColorSpaceTransform()
    {
    }
    
    ColorSpaceTransform& ColorSpaceTransform::operator= (const ColorSpaceTransform & rhs)
    {
        *m_impl = *rhs.m_impl;
        return *this;
    }
    
    TransformDirection ColorSpaceTransform::getDirection() const
    {
        return m_impl->dir_;
    }
    
    void ColorSpaceTransform::setDirection(TransformDirection dir)
    {
        m_impl->dir_ = dir;
    }
    
    const char * ColorSpaceTransform::getSrc() const
    {
        return m_impl->src_.c_str();
    }
    
    void ColorSpaceTransform::setSrc(const char * src)
    {
        m_impl->src_ = src;
    }
    
    const char * ColorSpaceTransform::getDst() const
    {
        return m_impl->dst_.c_str();
    }
    
    void ColorSpaceTransform::setDst(const char * dst)
    {
        m_impl->dst_ = dst;
    }
    
    std::ostream& operator<< (std::ostream& os, const ColorSpaceTransform& t)
    {
        os << "<ColorSpaceTransform ";
        os << "direction=" << TransformDirectionToString(t.getDirection()) << ", ";
        os << ">\n";
        return os;
    }
    
    
    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    
    void BuildColorSpaceOps(OpRcPtrVec & ops,
                            const Config& config,
                            const ColorSpaceTransform & colorSpaceTransform,
                            TransformDirection dir)
    {
        TransformDirection combinedDir = CombineTransformDirections(dir,
                                                  colorSpaceTransform.getDirection());
        
        ConstColorSpaceRcPtr src, dst;
        
        if(combinedDir == TRANSFORM_DIR_FORWARD)
        {
            src = config.getColorSpace( colorSpaceTransform.getSrc() );
            dst = config.getColorSpace( colorSpaceTransform.getDst() );
        }
        else if(combinedDir == TRANSFORM_DIR_INVERSE)
        {
            dst = config.getColorSpace( colorSpaceTransform.getSrc() );
            src = config.getColorSpace( colorSpaceTransform.getDst() );
        }
        
        BuildColorSpaceOps(ops, config, src, dst);
    }
    
    void BuildColorSpaceOps(OpRcPtrVec & ops,
                            const Config & config,
                            const ConstColorSpaceRcPtr & srcColorSpace,
                            const ConstColorSpaceRcPtr & dstColorSpace)
    {
        if(!srcColorSpace)
            throw Exception("BuildColorSpaceOps failed, null srcColorSpace.");
        if(!dstColorSpace)
            throw Exception("BuildColorSpaceOps failed, null dstColorSpace.");
        
        if(srcColorSpace->getFamily() == dstColorSpace->getFamily())
            return;
        if(dstColorSpace->isData() || srcColorSpace->isData())
            return;
        
        // Consider dt8 -> vd8?
        // One would have to explode the srcColorSpace->getTransform(COLORSPACE_DIR_TO_REFERENCE);
        // result, and walk through it step by step.  If the dstColorspace family were
        // ever encountered in transit, we'd want to short circuit the result.
        
        GpuAllocationData srcAllocation;
        srcAllocation.allocation = srcColorSpace->getGpuAllocation();
        srcAllocation.min = srcColorSpace->getGpuMin();
        srcAllocation.max = srcColorSpace->getGpuMax();
        
        CreateGpuAllocationNoOp(ops, srcAllocation);
        
        ConstTransformRcPtr toref = srcColorSpace->getTransform(COLORSPACE_DIR_TO_REFERENCE);
        BuildOps(ops, config, toref, TRANSFORM_DIR_FORWARD);
        
        // TODO: If ROLE_REFERENCE is defined, consider adding its GpuAllocation to the OpVec
        
        ConstTransformRcPtr fromref = dstColorSpace->getTransform(COLORSPACE_DIR_FROM_REFERENCE);
        BuildOps(ops, config, fromref, TRANSFORM_DIR_FORWARD);
        
        GpuAllocationData dstAllocation;
        dstAllocation.allocation = dstColorSpace->getGpuAllocation();
        dstAllocation.min = dstColorSpace->getGpuMin();
        dstAllocation.max = dstColorSpace->getGpuMax();
        
        CreateGpuAllocationNoOp(ops, dstAllocation);
    }
}
OCIO_NAMESPACE_EXIT
