/* @cpp.file.header */

/*  _________        _____ __________________        _____
 *  __  ____/___________(_)______  /__  ____/______ ____(_)_______
 *  _  / __  __  ___/__  / _  __  / _  / __  _  __ `/__  / __  __ \
 *  / /_/ /  _  /    _  /  / /_/ /  / /_/ /  / /_/ / _  /  _  / / /
 *  \____/   /_/     /_/   \_,__/   \____/   \__,_/  /_/   /_/ /_/
 */

#ifndef GRIDCLIENT_PORTABLE_MARSHALLER_HPP_INCLUDED
#define GRIDCLIENT_PORTABLE_MARSHALLER_HPP_INCLUDED

#include <vector>
#include <iterator>
#include <boost/algorithm/string.hpp>
#include <boost/detail/endian.hpp>

#include "gridgain/gridportable.hpp"
#include "gridgain/gridportablereader.hpp"
#include "gridgain/gridportablewriter.hpp"
#include "gridgain/impl/utils/gridclientbyteutils.hpp"
#include "gridgain/impl/marshaller/portable/gridclientportablemessages.hpp"
#include "gridgain/impl/cmd/gridclientmessagetopologyresult.hpp"
#include "gridgain/impl/cmd/gridclientmessagelogresult.hpp"
#include "gridgain/impl/cmd/gridclientmessagecacheresult.hpp"
#include "gridgain/impl/cmd/gridclientmessagecachemodifyresult.hpp"
#include "gridgain/impl/cmd/gridclientmessagecachemetricsresult.hpp"
#include "gridgain/impl/cmd/gridclientmessagecachegetresult.hpp"
#include "gridgain/impl/cmd/gridclientmessagetaskresult.hpp"

const int8_t TYPE_NULL = 0;
const int8_t TYPE_BYTE = 1;
const int8_t TYPE_SHORT = 2;
const int8_t TYPE_INT = 3;
const int8_t TYPE_LONG = 4;
const int8_t TYPE_FLOAT = 5;
const int8_t TYPE_DOUBLE = 6;
const int8_t TYPE_BOOLEAN = 7;
const int8_t TYPE_CHAR = 8;
const int8_t TYPE_STRING = 9;
const int8_t TYPE_BYTE_ARRAY = 10;

const int8_t TYPE_LIST = 18;
const int8_t TYPE_MAP = 19;
const int8_t TYPE_UUID = 20;
const int8_t TYPE_USER_OBJECT = 21;

const int8_t OBJECT_TYPE_OBJECT = 0;
const int8_t OBJECT_TYPE_NULL = 2;

const int8_t FLAG_NULL = 0x80;
const int8_t FLAG_HANDLE = 0x81;
const int8_t FLAG_OBJECT = 0x82;
const int8_t FLAG_METADATA = 0x83;

GridPortable* createPortable(int32_t typeId, GridPortableReader &reader);

class GridWriteHandleTable {
public:    
    GridWriteHandleTable(int initCap, float loadFactor) : spine(initCap, -1), next(initCap, 0), objs(initCap, nullptr), size(0) {
        this->loadFactor = loadFactor;

        threshold = (int)(initCap * loadFactor);
    }

    void clear() {
        std::fill(spine.begin(), spine.end(), -1); 
        
        std::fill(objs.begin(), objs.end(), nullptr); 

        size = 0;
    }

    int32_t lookup(void* obj) {
        int idx = hash(obj) % spine.size();

        if (size == 0) {
            assign(obj, idx);

            return -1;
        }

        for (int32_t i = spine[idx]; i >= 0; i = next[i]) {
            if (objs[i] == obj)
                return i;
        }

        assign(obj, idx);

        return -1;
    }

private:
    int hash(void* ptr) {
        return (size_t)ptr & 0x7FFFFFFF;            
    }

    void assign(void* obj, int idx) {
        if (size >= next.size())
            growEntries();

        if (size >= threshold) {
            growSpine();
        
            idx = hash(obj) % spine.size();
        }

        insert(obj, size, idx);

        size++;
    }

    void growEntries() {
        int newLen = (next.size() << 1) + 1;

        objs.resize(newLen);
        
        next.resize(newLen);
    }

    void growSpine() {
        int newLen = (spine.size() << 1) + 1;

        threshold = (int)(newLen * loadFactor);
        
        std::fill(spine.begin(), spine.end(), -1); 
        
        spine.resize(newLen, -1);

        for (int i = 0; i < this->size; i++) {
            void* obj = objs[i];

            int idx = hash(obj) % spine.size();

            insert(objs[i], i, idx);
        }
    }

    void insert(void* obj, int handle, int idx) {
        objs[handle] = obj;
        next[handle] = spine[idx];
        spine[idx] = handle;
    }

    int size;

    int threshold;

    float loadFactor;

    std::vector<int> spine;

    std::vector<int> next;

    std::vector<void*> objs;
};

class GridReadHandleTable {
public:
    GridReadHandleTable(int initCap) {
        handles.reserve(initCap);
    }

    int32_t assign(void* obj) {
        int32_t res = handles.size();

        handles.push_back(obj);

        return res;
    }

    void* lookup(int32_t handle) {
        return handles[handle];
    }

private:
    std::vector<void*> handles;
};

class PortableOutput {
public:
	PortableOutput(size_t cap) {
        out.reserve(cap);
	}

    virtual ~PortableOutput() {
    }

    void writeByte(int8_t val) {
        out.push_back(val);
    }

	void writeBytes(const void* src, size_t size) {
        const int8_t* bytes = reinterpret_cast<const int8_t*>(src);

        out.insert(out.end(), bytes, bytes + size);
	}

    virtual void writeInt16(int16_t val) = 0;

	virtual void writeInt32(int32_t val) = 0;

    virtual void writeInt64(int64_t val) = 0;

    virtual void writeFloat(float val) = 0;

    virtual void writeDouble(double val) = 0;

	std::vector<int8_t> bytes() {
        return out;
	}

protected:
    std::vector<int8_t> out;
};

class LittleEndianPortableOutput : public PortableOutput {
public:
	LittleEndianPortableOutput(size_t cap) : PortableOutput(cap) {
	}

	void writeInt16(int16_t val) override {
        int8_t* bytes = reinterpret_cast<int8_t*>(&val);

        out.insert(out.end(), bytes, bytes + 2);
	}

	void writeInt32(int32_t val) override {
        int8_t* bytes = reinterpret_cast<int8_t*>(&val);

        out.insert(out.end(), bytes, bytes + 4);
	}

    void writeInt64(int64_t val) override {
        int8_t* bytes = reinterpret_cast<int8_t*>(&val);

        out.insert(out.end(), bytes, bytes + 8);
	}

    void writeFloat(float val) override {
        int8_t* bytes = reinterpret_cast<int8_t*>(&val);

        out.insert(out.end(), bytes, bytes + 4);
	}

    void writeDouble(double val) override {
        int8_t* bytes = reinterpret_cast<int8_t*>(&val);

        out.insert(out.end(), bytes, bytes + 8);
	}
};

class BigEndianPortableOutput : public PortableOutput {
public:
	BigEndianPortableOutput(size_t cap) : PortableOutput(cap) {
	}

	void writeInt16(int16_t val) override {
        int8_t* bytes = reinterpret_cast<int8_t*>(&val);

        size_t size = out.size();

        out.resize(size + 2);

        std::reverse_copy(bytes, bytes + 2, out.data() + size);
	}

	void writeInt32(int32_t val) override {
        int8_t* bytes = reinterpret_cast<int8_t*>(&val);

        size_t size = out.size();

        out.resize(size + 4);

        std::reverse_copy(bytes, bytes + 4, out.data() + size);
	}

    void writeInt64(int64_t val) override {
        int8_t* bytes = reinterpret_cast<int8_t*>(&val);

        size_t size = out.size();

        out.resize(size + 8);

        std::reverse_copy(bytes, bytes + 8, out.data() + size);
	}

    void writeFloat(float val) override {
        int8_t* bytes = reinterpret_cast<int8_t*>(&val);

        size_t size = out.size();

        out.resize(size + 4);

        std::reverse_copy(bytes, bytes + 4, out.data() + size);
	}

    void writeDouble(double val) override {
        int8_t* bytes = reinterpret_cast<int8_t*>(&val);

        size_t size = out.size();

        out.resize(size + 8);

        std::reverse_copy(bytes, bytes + 8, out.data() + size);
	}
};

class GridPortableWriterImpl : public GridPortableWriter {
public:
	GridPortableWriterImpl() : out(1024) {
	}

    void writePortable(GridPortable &portable) {
        out.writeByte(OBJECT_TYPE_OBJECT);

        out.writeInt32(portable.typeId());

        portable.writePortable(*this);
    }

    void writeByte(char* fieldName, int8_t val) override {
		out.writeByte(val);
	}

    void writeInt16(char* fieldName, int16_t val) override {
		out.writeInt16(val);
	}
	
    void writeInt16Collection(char* fieldName, const std::vector<int16_t>& val) override {
        // TODO
    }
    
    void writeInt16Array(char* fieldName, int16_t* val, int32_t size) override {
        // TODO
    }

    void writeInt32(char* fieldName, int32_t val) override {
		out.writeInt32(val);
	}
	
    void writeInt32Collection(char* fieldName, const std::vector<int32_t>& val) override {
        // TODO
    }
    
    void writeInt32Array(char* fieldName, int32_t* val, int32_t size) override {
        // TODO
    }

    void writeInt64(char* fieldName, int64_t val) override {
		out.writeInt64(val);
	}
	
    void writeInt64Collection(char* fieldName, const std::vector<int64_t>& val) override {
        // TODO
    }
    
    void writeInt64Array(char* fieldName, int64_t* val, int32_t size) override {
        // TODO
    }

    void writeFloat(char* fieldName, float val) override {
		out.writeFloat(val);
	}
	
    void writeFloatCollection(char* fieldName, const std::vector<float>& val) override {
        // TODO
    }
    
    void writeFloatArray(char* fieldName, float* val, int32_t size) override {
        // TODO
    }

    void writeDouble(char* fieldName, double val) override {
		out.writeDouble(val);
	}
	
    void writeDoubleCollection(char* fieldName, const std::vector<double>& val) override {
        // TODO
    }
    
    void writeDoubleArray(char* fieldName, double* val, int32_t size) override {
        // TODO
    }

	void writeString(char* fieldName, const std::string &str) override {
		if (!str.empty()) {
            out.writeInt32(str.length());
		    out.writeBytes(str.data(), str.length());
        }
        else
            out.writeInt32(-1);
	}

    void writeStringCollection(char* fieldName, const std::vector<std::string>& val) override {
        // TODO
    }

    void writeWString(char* fieldName, const std::wstring& val) override {
        // TODO
    }

    void writeWStringCollection(char* fieldName, const std::vector<std::wstring>& val) override {
        // TODO
    }
	
    void writeByteCollection(char* fieldName, const std::vector<int8_t>& val) override {
        out.writeInt32(val.size());
        out.writeBytes(val.data(), val.size());
    }
    
    void writeByteArray(char* fieldName, int8_t* val, int32_t size) override {
        out.writeInt32(size);
        out.writeBytes(val, size);
    }

	void writeBool(char* fieldName, bool val) override {
		out.writeByte(val ? 1 : 0);
	}
	
    void writeBoolCollection(char* fieldName, const std::vector<bool>& val) override {
        // TODO
    }
    
    void writeBoolArray(char* fieldName, bool* val, int32_t size) override {
        // TODO
    }

	void writeUuid(char* fieldName, const boost::optional<GridClientUuid>& val) override {
        if (val) {
            out.writeByte(1);

            out.writeInt64(val.get().mostSignificantBits());
            out.writeInt64(val.get().leastSignificantBits());
        }
        else
            out.writeByte(0);
	}

    void writeVariant(char* fieldName, const GridClientVariant &val) override {
        if (val.hasInt()) {
            out.writeByte(TYPE_INT);
            out.writeInt32(val.getInt());
        }
        else if (val.hasString()) {
            out.writeByte(TYPE_STRING);

            std::string str = val.getString();

            writeString(nullptr, str);
        }
        else if (val.hasUuid()) {
            out.writeByte(TYPE_UUID);

            GridClientUuid uuid = val.getUuid();

            out.writeInt64(uuid.mostSignificantBits());
            out.writeInt64(uuid.leastSignificantBits());
        }
        else if (val.hasPortable()) {
            out.writeByte(TYPE_USER_OBJECT);

            writePortable(*val.getPortable<GridPortable>());
        }
        else if (val.hasVariantVector()) {
            out.writeByte(TYPE_LIST);

            writeCollection(val.getVariantVector());
        }
        else if (val.hasVariantMap()) {
            out.writeByte(TYPE_MAP);

            writeMap(val.getVariantMap());
        }
        else if (!val.hasAnyValue()) {
            out.writeByte(TYPE_NULL);
        }
        else {
            assert(false);
        }
    }
    
    void writeVariantCollection(char* fieldName, const TGridClientVariantSet &val) override {
        writeCollection(val);
    }

    void writeVariantMap(char* fieldName, const TGridClientVariantMap &map) override {
        writeMap(map);
    }

    void writeByte(int8_t val) override {
        // TODO
	}

    void writeInt16(int16_t val) override {
        // TODO
	}
	
    void writeInt16Collection(const std::vector<int16_t>& val) override {
        // TODO
    }
    
    void writeInt16Array(int16_t* val, int32_t size) override {
        // TODO
    }

    void writeInt32(int32_t val) override {
        // TODO
	}
	
    void writeInt32Collection(const std::vector<int32_t>& val) override {
        // TODO
    }
    
    void writeInt32Array(int32_t* val, int32_t size) override {
        // TODO
    }

    void writeInt64(int64_t val) override {
        // TODO
	}
	
    void writeInt64Collection(const std::vector<int64_t>& val) override {
        // TODO
    }
    
    void writeInt64Array(int64_t* val, int32_t size) override {
        // TODO
    }

    void writeFloat(float val) override {
        // TODO
	}
	
    void writeFloatCollection(const std::vector<float>& val) override {
        // TODO
    }
    
    void writeFloatArray(float* val, int32_t size) override {
        // TODO
    }

    void writeDouble(double val) override {
        // TODO
	}
	
    void writeDoubleCollection(const std::vector<double>& val) override {
        // TODO
    }
    
    void writeDoubleArray(double* val, int32_t size) override {
        // TODO
    }

	void writeString(const std::string &str) override {
        // TODO
	}

    void writeStringCollection(const std::vector<std::string>& val) override {
        // TODO
    }

    void writeWString(const std::wstring& val) override {
        // TODO
    }

    void writeWStringCollection(const std::vector<std::wstring>& val) override {
        // TODO
    }
	
    void writeByteCollection(const std::vector<int8_t>& val) override {
        // TODO
    }
    
    void writeByteArray(int8_t* val, int32_t size) override {
        // TODO
    }

	void writeBool(bool val) override {
        // TODO
	}
	
    void writeBoolCollection(const std::vector<bool>& val) override {
        // TODO
    }
    
    void writeBoolArray(bool* val, int32_t size) override {
        // TODO
    }

	void writeUuid(const boost::optional<GridClientUuid>& val) override {
        // TODO
	}

    void writeVariant(const GridClientVariant &val) override {
        // TODO
    }
    
    void writeVariantCollection(const TGridClientVariantSet &val) override {
        // TODO
    }

    void writeVariantMap(const TGridClientVariantMap &map) override {
        // TODO
    }

	std::vector<int8_t> bytes() {
		return out.bytes();
	}

private:
    void writeCollection(const TGridClientVariantSet& col) {
        out.writeByte(OBJECT_TYPE_OBJECT);

        out.writeInt32(col.size());

        for (auto iter = col.begin(); iter != col.end(); ++iter) {
            GridClientVariant variant = *iter;

            writeVariant(nullptr, variant);
        }
    }

    void writeMap(const TGridClientVariantMap& map) {
        out.writeByte(OBJECT_TYPE_OBJECT);

        out.writeInt32(map.size());

        for (auto iter = map.begin(); iter != map.end(); ++iter) {
            GridClientVariant key = iter->first;
            GridClientVariant val = iter->second;

            writeVariant(nullptr, key);
            writeVariant(nullptr, val);
        }
    }

#ifdef BOOST_BIG_ENDIAN
    BigEndianPortableOutput out;
#else
    LittleEndianPortableOutput out;
#endif;
};

class PortableInput {
public:
    PortableInput(std::vector<int8_t>& data) : bytes(data), pos(0) {
    }

    virtual ~PortableInput() {
    }

    int8_t readByte() {
        checkAvailable(1);

        return bytes[pos++];
    }

    std::vector<int8_t> readBytes(int32_t size) {
        checkAvailable(size);

        std::vector<int8_t> vec;

        vec.insert(vec.end(), bytes.data() + pos, bytes.data() + pos + size);

        pos += size;

        return vec;
    }

    int8_t* readBytesArray(int32_t size) {
        checkAvailable(size);

        int8_t* res = new int8_t[size];

        memcpy(res, bytes.data() + pos, size);

        pos += size;

        return res;
    }

    virtual int16_t readInt16() = 0;

    virtual int32_t readInt32() = 0;

    virtual int64_t readInt64() = 0;

    virtual float readFloat() = 0;

    virtual double readDouble() = 0;

protected:
    void checkAvailable(size_t cnt) {
        assert(pos + cnt <= bytes.size());
    }
    
    int32_t pos;

    std::vector<int8_t>& bytes;
};

class LittleEndianPortableInput : public PortableInput {
public:
    LittleEndianPortableInput(std::vector<int8_t>& data) : PortableInput(data) {
    }

    int16_t readInt16() override {
        checkAvailable(2);

        int16_t res;

        int8_t* start = bytes.data() + pos;
        int8_t* end = start + 2;

        int8_t* dst = reinterpret_cast<int8_t*>(&res);

        std::copy(start, end, dst);

        pos += 2;

        return res;
    }

    int32_t readInt32() override {
        checkAvailable(4);

        int32_t res;

        int8_t* start = bytes.data() + pos;
        int8_t* end = start + 4;

        int8_t* dst = reinterpret_cast<int8_t*>(&res);

        std::copy(start, end, dst);

        pos += 4;

        return res;
    }

    int64_t readInt64() override {
        checkAvailable(8);

        int64_t res;

        int8_t* start = bytes.data() + pos;
        int8_t* end = start + 8;

        int8_t* dst = reinterpret_cast<int8_t*>(&res);

        std::copy(start, end, dst);

        pos += 8;

        return res;
    }

    float readFloat() override {
        checkAvailable(4);

        float res;

        int8_t* start = bytes.data() + pos;
        int8_t* end = start + 4;

        int8_t* dst = reinterpret_cast<int8_t*>(&res);

        std::copy(start, end, dst);

        pos += 4;

        return res;
    }

    double readDouble() override {
        checkAvailable(4);

        double res;

        int8_t* start = bytes.data() + pos;
        int8_t* end = start + 8;

        int8_t* dst = reinterpret_cast<int8_t*>(&res);

        std::copy(start, end, dst);

        pos += 8;

        return res;
    }
};

class BigEndianPortableInput : public PortableInput {
public:
    BigEndianPortableInput(std::vector<int8_t>& data) : PortableInput(data) {
    }

    int16_t readInt16() override {
        checkAvailable(2);

        int16_t res;

        int8_t* start = bytes.data() + pos;
        int8_t* end = start + 2;

        int8_t* dst = reinterpret_cast<int8_t*>(&res);

        std::reverse_copy(start, end, dst);

        pos += 2;

        return res;
    }

    int32_t readInt32() override {
        checkAvailable(4);

        int32_t res;

        int8_t* start = bytes.data() + pos;
        int8_t* end = start + 4;

        int8_t* dst = reinterpret_cast<int8_t*>(&res);

        std::reverse_copy(start, end, dst);

        pos += 4;

        return res;
    }

    int64_t readInt64() override {
        checkAvailable(8);

        int64_t res;

        int8_t* start = bytes.data() + pos;
        int8_t* end = start + 8;

        int8_t* dst = reinterpret_cast<int8_t*>(&res);

        std::reverse_copy(start, end, dst);

        pos += 8;

        return res;
    }

    float readFloat() override {
        checkAvailable(4);

        float res;

        int8_t* start = bytes.data() + pos;
        int8_t* end = start + 4;

        int8_t* dst = reinterpret_cast<int8_t*>(&res);

        std::reverse_copy(start, end, dst);

        pos += 4;

        return res;
    }

    double readDouble() override {
        checkAvailable(4);

        double res;

        int8_t* start = bytes.data() + pos;
        int8_t* end = start + 8;

        int8_t* dst = reinterpret_cast<int8_t*>(&res);

        std::reverse_copy(start, end, dst);

        pos += 8;

        return res;
    }
};

class GridPortableReaderImpl : GridPortableReader {
public:
    GridPortableReaderImpl(std::vector<int8_t>& data) : in(data) {
    }

    GridPortable* readPortable() {
        int8_t type = in.readByte();

        assert(type == OBJECT_TYPE_OBJECT);

        int32_t typeId = in.readInt32();

        GridPortable* portable = createPortable(typeId, *this);

        return portable;
    }

    int8_t readByte(char* fieldName) override {
        return in.readByte();
    }

    boost::optional<std::vector<int8_t>> readByteCollection(char* fieldName) override {
        int32_t size = in.readInt32();

        if (size > 0)
            return in.readBytes(size);

        return std::vector<int8_t>();
    }

    std::pair<int8_t*, int32_t> readByteArray(char* fieldName) override {
        int32_t size = in.readInt32();

        if (size > 0)
            return std::pair<int8_t*, int32_t>(in.readBytesArray(size), size);

        return std::pair<int8_t*, int32_t>(nullptr, -1);
    }

    int16_t readInt16(char* fieldName) override {
        return in.readInt32();
    }

    std::pair<int16_t*, int32_t> readInt16Array(char* fieldName) override {
        return std::pair<int16_t*, int32_t>(nullptr, -1); // TODO
    }

    boost::optional<std::vector<int16_t>> readInt16Collection(char* fieldName) override {
        return std::vector<int16_t>(); // TODO
    }

    int32_t readInt32(char* fieldName) override {
        return in.readInt32();
    }

    std::pair<int32_t*, int32_t> readInt32Array(char* fieldName) override {
        return std::pair<int32_t*, int32_t>(nullptr, -1); // TODO
    }

    boost::optional<std::vector<int32_t>> readInt32Collection(char* fieldName) override {
        return std::vector<int32_t>(); // TODO
    }

    int64_t readInt64(char* fieldName) override {
        return in.readInt64();
    }

    std::pair<int64_t*, int32_t> readInt64Array(char* fieldName) override {
        return std::pair<int64_t*, int32_t>(nullptr, -1); // TODO
    }

    boost::optional<std::vector<int64_t>> readInt64Collection(char* fieldName) override {
        return std::vector<int64_t>(); // TODO
    }

    float readFloat(char* fieldName) override {
        return in.readFloat();
    }

    std::pair<float*, int32_t> readFloatArray(char* fieldName) override {
        return std::pair<float*, int32_t>(nullptr, -1); // TODO
    }

    boost::optional<std::vector<float>> readFloatCollection(char* fieldName) override {
        return std::vector<float>(); // TODO
    }

    double readDouble(char* fieldName) override {
        return in.readDouble();
    }

    std::pair<double*, int32_t> readDoubleArray(char* fieldName) override {
        return std::pair<double*, int32_t>(nullptr, -1); // TODO
    }

    boost::optional<std::vector<double>> readDoubleCollection(char* fieldName) override {
        return std::vector<double>(); // TODO
    }

    boost::optional<std::string> readString(char* fieldName) override {
        boost::optional<std::string> res;

        int size = in.readInt32();

        if (size == -1)
            return res;

        std::vector<int8_t> bytes = in.readBytes(size);

        res.reset(std::string((char*)bytes.data(), size));

        return res;
    }

    boost::optional<std::vector<std::string>> readStringCollection(char* fieldName) override {
        return std::vector<std::string>(); // TODO
    }

    boost::optional<std::wstring> readWString(char* fieldName) override {
        return std::wstring(); // TODO
    }

    boost::optional<std::vector<std::wstring>> readWStringCollection(char* fieldName) override {
        return std::vector<std::wstring>(); // TODO
    }

    bool readBool(char* fieldName) override {
        int8_t val = in.readByte();

        return val == 0 ? false : true;
    }

    boost::optional<std::vector<bool>> readBoolCollection(char* fieldName) override {
        return std::vector<bool>(); // TODO
    }

    std::pair<bool*, int32_t> readBoolArray(char* fieldName) override {
        return std::pair<bool*, int32_t>(nullptr, -1); // TODO
    }

    boost::optional<GridClientUuid> readUuid(char* fieldName) override {
        boost::optional<GridClientUuid> res;

        if (in.readByte() != 0) {
            int64_t mostSignificantBits = in.readInt64();
            int64_t leastSignificantBits = in.readInt64();

            res = GridClientUuid(mostSignificantBits, leastSignificantBits);
        }

        return res;
    }

    GridClientVariant readVariant(char* fieldName) override {
        int8_t type = in.readByte();

        switch (type) {
            case TYPE_NULL:
                return GridClientVariant();

            case TYPE_INT:
                return GridClientVariant(in.readInt32());

            case TYPE_BOOLEAN:
                return GridClientVariant(in.readByte() != 0);

            case TYPE_STRING:
                return GridClientVariant(readString(nullptr).get());

            case TYPE_LONG:
                return GridClientVariant(in.readInt64());

            case TYPE_UUID: {
                if (in.readByte() == 0)
                    return GridClientVariant();

                int64_t mostSignificantBits = in.readInt64();
                int64_t leastSignificantBits = in.readInt64();

                return GridClientVariant(GridClientUuid(mostSignificantBits, leastSignificantBits));
            }

            case TYPE_LIST:
                return GridClientVariant(readCollection().get());

            case TYPE_MAP:
                return GridClientVariant(readMap().get());

            case TYPE_USER_OBJECT:
                return GridClientVariant(readPortable());

            default:
                assert(false);
        }

        return GridClientVariant();
    }

    boost::optional<TGridClientVariantSet> readVariantCollection(char* fieldName) override {
        return readCollection();
    }

    boost::optional<TGridClientVariantMap> readVariantMap(char* fieldName) override {
        return readMap();
    }

    int8_t readByte() override {
        return 0; // TODO
    }

    boost::optional<std::vector<int8_t>> readByteCollection() override {
        return std::vector<int8_t>(); // TODO
    }

    std::pair<int8_t*, int32_t> readByteArray() override {
        return std::pair<int8_t*, int32_t>(nullptr, -1); // TODO
    }

    int16_t readInt16() override {
        return 0; // TODO
    }

    std::pair<int16_t*, int32_t> readInt16Array() override {
        return std::pair<int16_t*, int32_t>(nullptr, -1); // TODO
    }

    boost::optional<std::vector<int16_t>> readInt16Collection() override {
        return std::vector<int16_t>(); // TODO
    }

    int32_t readInt32() override {
        return 0; // TODO
    }

    std::pair<int32_t*, int32_t> readInt32Array() override {
        return std::pair<int32_t*, int32_t>(nullptr, -1); // TODO
    }

    boost::optional<std::vector<int32_t>> readInt32Collection() override {
        return std::vector<int32_t>(); // TODO
    }

    int64_t readInt64() override {
        return 0; // TODO
    }

    std::pair<int64_t*, int32_t> readInt64Array() override {
        return std::pair<int64_t*, int32_t>(nullptr, -1); // TODO
    }

    boost::optional<std::vector<int64_t>> readInt64Collection() override {
        return std::vector<int64_t>(); // TODO
    }

    float readFloat() override {
        return in.readFloat(); // TODO
    }

    std::pair<float*, int32_t> readFloatArray() override {
        return std::pair<float*, int32_t>(nullptr, -1); // TODO
    }

    boost::optional<std::vector<float>> readFloatCollection() override {
        return std::vector<float>(); // TODO
    }

    double readDouble() override {
        return in.readDouble();
    }

    std::pair<double*, int32_t> readDoubleArray() override {
        return std::pair<double*, int32_t>(nullptr, -1); // TODO
    }

    boost::optional<std::vector<double>> readDoubleCollection() override {
        return std::vector<double>(); // TODO
    }

    boost::optional<std::string> readString() override { 
        return std::string(); // TODO
    }

    boost::optional<std::vector<std::string>> readStringCollection() override {
        return std::vector<std::string>(); // TODO
    }

    boost::optional<std::wstring> readWString() override {
        return std::wstring(); // TODO
    }

    boost::optional<std::vector<std::wstring>> readWStringCollection() override {
        return std::vector<std::wstring>(); // TODO
    }

    bool readBool() override {
        return false; // TODO
    }

    boost::optional<std::vector<bool>> readBoolCollection() override {
        return std::vector<bool>(); // TODO
    }

    std::pair<bool*, int32_t> readBoolArray() override {
        return std::pair<bool*, int32_t>(nullptr, -1); // TODO
    }

    boost::optional<GridClientUuid> readUuid() override {
        boost::optional<GridClientUuid> res;

        return res; // TODO
    }

    GridClientVariant readVariant() override {
        return GridClientVariant(); // TODO
    }

    boost::optional<TGridClientVariantSet> readVariantCollection() override {
        return readCollection(); // TODO
    }

    boost::optional<TGridClientVariantMap> readVariantMap() override {
        return readMap(); // TODO
    }

private:
    boost::optional<TGridClientVariantSet> readCollection() {
        boost::optional<TGridClientVariantSet> res;

        int8_t type = in.readByte();

        assert(type == OBJECT_TYPE_OBJECT);

        int32_t size = in.readInt32();

        if (size == -1)
            return res;

        std::vector<GridClientVariant> vec;

        for (int i = 0; i < size; i++)
            vec.push_back(readVariant(nullptr));

        res.reset(vec);

        return res;
    }

    boost::optional<TGridClientVariantMap> readMap() {
        boost::optional<TGridClientVariantMap> res;

        int8_t type = in.readByte();

        if (type == OBJECT_TYPE_NULL)
            return res;

        assert(type == OBJECT_TYPE_OBJECT);

        int32_t size = in.readInt32();

        if (size == -1)
            return boost::unordered_map<GridClientVariant, GridClientVariant>();
        
        boost::unordered_map<GridClientVariant, GridClientVariant> map;

        for (int i = 0; i < size; i++) {
            GridClientVariant key = readVariant(nullptr);

            GridClientVariant val = readVariant(nullptr);

            map[key] = val;
        }

        res.reset(map);

        return res;
    }

#ifdef BOOST_BIG_ENDIAN
    BigEndianPortableInput in;
#else
    LittleEndianPortableInput in;
#endif;
};

class GridPortableMarshaller {
public:
    std::vector<int8_t> marshal(GridPortable& portable) {
		GridPortableWriterImpl writer;

		writer.writePortable(portable);

		return writer.bytes();
	}

    template<typename T>
    T* unmarshal(std::vector<int8_t>& data) {
		GridPortableReaderImpl reader(data);

        return static_cast<T*>(reader.readPortable());
    }

    void parseResponse(GridClientResponse* msg, GridClientMessageTopologyResult& resp) {
        GridClientVariant res = msg->getResult();

        assert(res.hasVariantVector());

        std::vector<GridClientVariant> vec = res.getVariantVector();

        std::vector<GridClientNode> nodes;

        for (auto iter = vec.begin(); iter != vec.end(); ++iter) {
            GridClientVariant nodeVariant = *iter;

            GridClientNodeBean* nodeBean = nodeVariant.getPortable<GridClientNodeBean>();
        
            nodes.push_back(nodeBean->createNode());

            delete nodeBean;
        }

        resp.setNodes(nodes);
    }

    void parseResponse(GridClientResponse* msg, GridClientMessageLogResult& resp) {
        GridClientVariant res = msg->getResult();

        assert(res.hasVariantVector());

        std::vector<GridClientVariant> vec = res.getVariantVector();

        std::vector<std::string> lines;
        
        for (auto iter = vec.begin(); iter != vec.end(); ++iter) {
            GridClientVariant line = *iter;

            assert(line.hasString());

            lines.push_back(line.getString());
        }

        resp.lines(lines);
    }

    void parseResponse(GridClientResponse* msg, GridClientMessageCacheGetResult& resp) {
        GridClientVariant res = msg->getResult();

        if (res.hasVariantMap()) {
            TCacheValuesMap keyValues = res.getVariantMap();
            
            resp.setCacheValues(keyValues);
        }
        else {
            TCacheValuesMap keyValues;

            keyValues.insert(std::make_pair(GridClientVariant(), res));

            resp.setCacheValues(keyValues);
        }
    }

    void parseResponse(GridClientResponse* msg, GridClientMessageCacheModifyResult& resp) {
        GridClientVariant res = msg->getResult();

        if (res.hasBool())
            resp.setOperationResult(res.getBool());
        else
            resp.setOperationResult(false);
    }

    void parseResponse(GridClientResponse* msg, GridClientMessageCacheMetricResult& resp) {
        GridClientVariant res = msg->getResult();

        assert(res.hasPortable());

        std::unique_ptr<GridClientCacheMetricsBean> metricsBean(res.getPortable<GridClientCacheMetricsBean>());

        TCacheMetrics metrics;

        metrics["createTime"] = (*metricsBean).createTime;
        metrics["readTime"] = (*metricsBean).readTime;
        metrics["writeTime"] = (*metricsBean).writeTime;
        metrics["reads"] = (*metricsBean).reads;
        metrics["writes"] = (*metricsBean).writes;
        metrics["hits"] = (*metricsBean).hits;
        metrics["misses"] = (*metricsBean).misses;

        resp.setCacheMetrics(metrics);
    }
    
    void parseResponse(GridClientResponse* msg, GridClientMessageTaskResult& resp) {
        GridClientVariant res = msg->getResult();

        if (res.hasPortable()) {
            assert(res.getPortable()->typeId() == -8);

            GridClientTaskResultBean* resBean = res.getPortable<GridClientTaskResultBean>();

            resp.setTaskResult(resBean->res);

            delete resBean;
        }
    }
};

#endif
