#include "core/MySqlDbWrapper.h"
#include "core/MySqlResultReader.h"
#include "core/MySqlTypesHelper.h"
#include "core/DbType.h"
#include "utils/MemoryUtils.h"

using namespace std;

namespace ImgDetective {
namespace Core {
namespace Db {

    CTOR MySqlDbWrapper::MySqlDbWrapper(const REF MySqlConnectionSettings& conSettings) {
        this->conSettings = conSettings;
    }

    void MySqlDbWrapper::ExecuteNonQuery(const REF std::string& sqlStatement, const REF params_list_t& params) const {
        MYSQL* con = Connect();
        
        MYSQL_STMT* stmt = mysql_stmt_init(con);

        if (stmt == NULL) {
            throw std::exception("couldn't allocate MYSQL_STMT structure");
        }

        MYSQL_BIND* bind = NULL;

        try {
            if (mysql_stmt_prepare(stmt, sqlStatement.c_str(), sqlStatement.length())) {
                throw std::exception("mysql_stmt_prepare failed");
            }

            bind = PrepareParams(params);

            if (mysql_stmt_bind_param(stmt, bind)) {
                throw std::exception("mysql_stmt_bind_param failed");
            }

            if (mysql_stmt_execute(stmt)) {
                throw std::exception("mysql_stmt_execute failed");
            }
        }
        catch (...) {
            mysql_stmt_close(stmt);
            Utils::Memory::SafeDeleteArray(bind);
            Disconnect(con);
            throw;
        }

        mysql_stmt_close(stmt);
        Utils::Memory::SafeDeleteArray(bind);
        Disconnect(con);
    }

    DbResultReader* MySqlDbWrapper::ExecuteReader(const std::string& sqlStatement, const params_list_t& params) const {
        MYSQL* connection = NULL;
        MYSQL_STMT* stmt = NULL;
        MYSQL_BIND* resultBind = NULL;
        fields_vector_t* fieldBuffers = NULL;
        MYSQL_RES* resultMetadata = NULL;
        try {
            ExecuteStatement(sqlStatement, params, OUT connection, OUT stmt);

            //at this point, we've got to prepare buffers and pass them along with the stmt struct to the result reader

            /* the column count is > 0 if there is a result set 
            0 if the result is only the final status packet */
            unsigned int fieldsCount = mysql_stmt_field_count(stmt);

            if (fieldsCount > 0) {
                resultMetadata = mysql_stmt_result_metadata(stmt);
                PrepareFieldBuffers(resultMetadata, fieldBuffers, resultBind);

                if (mysql_stmt_bind_result(stmt, resultBind)) {
                    throw std::exception("mysql_stmt_bind_result failed");
                }

                return new MySqlResultReader(fieldBuffers, resultBind, stmt);
            }

            //mysql_stmt_bind_result(stmt, 
        }
        catch (...) {
            Utils::Memory::SafeDeleteArray(resultBind);
            mysql_stmt_close(stmt);
            stmt = NULL;
            Disconnect(connection);
            connection = NULL;
            if (resultMetadata != NULL) {
                mysql_free_result(resultMetadata);
            }
            throw;
        }

        Utils::Memory::SafeDeleteArray(resultBind);
        if (resultMetadata != NULL) {
            mysql_free_result(resultMetadata);
        }
    }

    void MySqlDbWrapper::ExecuteStatement(const std::string& sqlStatement, const params_list_t& params, OUT MYSQL*& connection, OUT MYSQL_STMT*& stmt) const {
        //connect to the db server
        connection = Connect();
        
        //allocate some memory for stmt struct
        MYSQL_STMT* stmt = mysql_stmt_init(connection);

        if (stmt == NULL) {
            throw std::exception("couldn't allocate MYSQL_STMT structure");
        }

        MYSQL_BIND* bind = NULL;

        try {
            //prepare stmt struct, passing the specified sql statement to it
            if (mysql_stmt_prepare(stmt, sqlStatement.c_str(), sqlStatement.length())) {
                throw std::exception("mysql_stmt_prepare failed");
            }

            //prepare bind array used by mysql to pass the specified params to the db server
            bind = PrepareParams(params);

            //connect bind array to stmt struct
            if (mysql_stmt_bind_param(stmt, bind)) {
                throw std::exception("mysql_stmt_bind_param failed");
            }

            //and finally let's execute the statement
            if (mysql_stmt_execute(stmt)) {
                throw std::exception("mysql_stmt_execute failed");
            }
        }
        catch (...) {
            mysql_stmt_close(stmt);
            stmt = NULL;
            Utils::Memory::SafeDeleteArray(bind);
            Disconnect(connection);
            connection = NULL;
            throw;
        }

        //we don't need array of bind structures for input params anymore
        Utils::Memory::SafeDeleteArray(bind);

        //now it's up to the calling routine to process the results of the statement's execution
    }

    MYSQL* MySqlDbWrapper::Connect() const {
        MYSQL* con = mysql_init(NULL);
        
        if (con == NULL) {
            throw std::exception("couldn't allocate MYSQL object");
        }

        if (mysql_real_connect(con, conSettings.host.c_str(), conSettings.login.c_str(), conSettings.password.c_str(), conSettings.dbName.c_str(), conSettings.port, NULL, 0) == NULL) {
            mysql_close(con);
            throw std::exception("couldn't connect to mysql database");
        }

        return con;
    }

    void MySqlDbWrapper::Disconnect(MYSQL* con) const {
        if (con != NULL) {
            mysql_close(con);
        }
    }

    MYSQL_BIND* MySqlDbWrapper::PrepareParams(const params_list_t& params) {
        MYSQL_BIND* bind = new MYSQL_BIND[params.size()];
        
        params_list_t::const_iterator it;
        size_t paramIndex = 0;
        for (it = params.cbegin(); it != params.cend(); ++it) {
            bind[paramIndex] = PrepareParamInfo(*it);
        }

        return bind;
    }

    void MySqlDbWrapper::PrepareFieldBuffers(MYSQL_RES* resultMetadata, OUT fields_vector_t*& fieldBuffers, OUT MYSQL_BIND*& bindArray) {
        Utils::Contract::AssertNotNull(resultMetadata);
        Utils::Contract::AssertIsNull(fieldBuffers);
        Utils::Contract::AssertIsNull(bindArray);

        unsigned int fieldCount = resultMetadata->field_count;

        try {
            fieldBuffers = new fields_vector_t();
            fieldBuffers->resize(fieldCount);
            bindArray = new MYSQL_BIND[fieldCount];

            MYSQL_FIELD* fields = mysql_fetch_fields(resultMetadata);

            //for each field in the result metadata we associate MYSQL_BIND struct and 
            //FieldBuffer struct pointing to the same memory location
            for (int i = 0; i < fieldCount; ++i) {
                MYSQL_FIELD fieldMetadata = fields[i];
                DbFieldBuffer& uniBuffer = (*fieldBuffers)[i];
                InitUniFieldBufferFromMySqlFieldMetadata(fieldMetadata, REF uniBuffer);
                bindArray[i] = CreateMySqlBindFromUniFieldBuffer(uniBuffer, fieldMetadata);
            }
        }
        catch (...) {
            Utils::Memory::SafeDelete(fieldBuffers);
            Utils::Memory::SafeDeleteArray(bindArray);
            throw;
        }
    }

    MYSQL_BIND MySqlDbWrapper::PrepareParamInfo(const REF DbParamBuffer& paramInfo) {
        MYSQL_BIND bind;
        memset(&bind, 0, sizeof(MYSQL_BIND));

        enum_field_types mysqlType = MySqlTypesHelper::GetMySqlType(paramInfo.GetType());
        bind.buffer_type = mysqlType;
        unsigned int typeLength = MySqlTypesHelper::GetMySqlTypeLength(mysqlType);

        if (typeLength != 0) {
            bind.buffer_length = typeLength;
        }
        else {
            //variable length param
            bind.buffer_length = paramInfo.GetDataLength();
        }

        bind.buffer = paramInfo.GetDataPtr();

        return bind;
    }

    void MySqlDbWrapper::InitUniFieldBufferFromMySqlFieldMetadata(const MYSQL_FIELD& fieldMetadata, REF DbFieldBuffer& uniBuf) {
        uniBuf.SetFieldName(fieldMetadata.name);

        //get universal type
        DbType::Enum dbType = MySqlTypesHelper::GetDbTypeFromMySqlType(fieldMetadata.type);
        uniBuf.SetType(dbType);

        unsigned int fieldLength = MySqlTypesHelper::GetMySqlTypeLength(fieldMetadata.type);

        if (fieldLength != 0) {
            //if length of the field is constant we allocate it beforehand
            uniBuf.Allocate(fieldLength);
            uniBuf.SetIsOfVariableLength(false);
        }
        else {
            uniBuf.SetIsOfVariableLength(true);
        }
    }

    MYSQL_BIND MySqlDbWrapper::CreateMySqlBindFromUniFieldBuffer(const DbFieldBuffer& uniBuf, const MYSQL_FIELD& fieldMetadata) {
        MYSQL_BIND mysqlBuf;
        memset(&mysqlBuf, 0, sizeof(mysqlBuf));

        if (uniBuf.IsOfVariableLength()) {
            mysqlBuf.buffer = NULL;
            mysqlBuf.buffer_length = 0;
        }
        else {
            mysqlBuf.buffer = uniBuf.GetDataPtr();
            mysqlBuf.buffer_length = uniBuf.GetDataLength();
        }

        mysqlBuf.buffer_type = fieldMetadata.type;
    }
}
}
}