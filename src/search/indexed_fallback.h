#pragma once
#include <windows.h>
#include <oledb.h>
#include <msdasc.h>
#include <winrt/base.h>
#include <atomic>
#include "model.h"
namespace delight::search {
// Queries the existing Windows index. Never crawls folders or creates an index.
inline std::vector<Result> indexedFallback(const std::wstring& query,unsigned generation,const std::atomic<unsigned>& latest,std::wstring& status){
    std::vector<Result> results;auto terms=tokens(query);if(terms.empty())return results;
    try{
        auto init=winrt::create_instance<IDataInitialize>(CLSID_MSDAINITIALIZE);winrt::com_ptr<IDBInitialize> db;
        winrt::check_hresult(init->GetDataSource(nullptr,CLSCTX_INPROC_SERVER,L"Provider=Search.CollatorDSO;Extended Properties='Application=Windows';",IID_IDBInitialize,reinterpret_cast<IUnknown**>(db.put())));
        winrt::check_hresult(db->Initialize());auto sessions=db.as<IDBCreateSession>();winrt::com_ptr<IDBCreateCommand> session;winrt::check_hresult(sessions->CreateSession(nullptr,IID_IDBCreateCommand,reinterpret_cast<IUnknown**>(session.put())));
        winrt::com_ptr<ICommandText> command;winrt::check_hresult(session->CreateCommand(nullptr,IID_ICommandText,reinterpret_cast<IUnknown**>(command.put())));
        if(auto properties=command.try_as<ICommandProperties>()){DBPROP timeout{};timeout.dwPropertyID=DBPROP_COMMANDTIMEOUT;timeout.dwOptions=DBPROPOPTIONS_OPTIONAL;timeout.vValue.vt=VT_I4;timeout.vValue.lVal=1;DBPROPSET set{&timeout,1,DBPROPSET_ROWSET};properties->SetProperties(1,&set);}
        std::wstring sql=L"SELECT TOP 80 System.ItemNameDisplay, System.ItemPathDisplay, System.FileAttributes FROM SYSTEMINDEX WHERE scope='file:'";
        for(auto& term:terms){std::wstring escaped;for(auto c:term){if(c==L'\'')escaped+=L'\'';escaped+=c;}sql+=L" AND System.ItemPathDisplay LIKE '%"+escaped+L"%'";}
        winrt::check_hresult(command->SetCommandText(DBGUID_DEFAULT,sql.c_str()));winrt::com_ptr<IRowset> rows;winrt::check_hresult(command->Execute(nullptr,IID_IRowset,nullptr,nullptr,reinterpret_cast<IUnknown**>(rows.put())));
        struct Row {DBSTATUS nameStatus,pathStatus,attributeStatus;wchar_t name[512],path[4096];DWORD attributes;};
        DBBINDING bindings[3]{};size_t values[]={offsetof(Row,name),offsetof(Row,path),offsetof(Row,attributes)},statuses[]={offsetof(Row,nameStatus),offsetof(Row,pathStatus),offsetof(Row,attributeStatus)};DBLENGTH lengths[]={sizeof(Row::name),sizeof(Row::path),sizeof(DWORD)};
        for(int i=0;i<3;++i){auto& b=bindings[i];b.iOrdinal=i+1;b.obValue=values[i];b.obStatus=statuses[i];b.dwPart=DBPART_VALUE|DBPART_STATUS;b.dwMemOwner=DBMEMOWNER_CLIENTOWNED;b.eParamIO=DBPARAMIO_NOTPARAM;b.cbMaxLen=lengths[i];b.wType=DBTYPE(i==2?DBTYPE_UI4:DBTYPE_WSTR);}
        auto accessor=rows.as<IAccessor>();HACCESSOR binding{};winrt::check_hresult(accessor->CreateAccessor(DBACCESSOR_ROWDATA,3,bindings,sizeof(Row),&binding,nullptr));
        while(results.size()<80&&generation==latest.load()){DBCOUNTITEM count=0;HROW* handles=nullptr;HRESULT hr=rows->GetNextRows(DB_NULL_HCHAPTER,0,1,&count,&handles);if(FAILED(hr)||!count){CoTaskMemFree(handles);break;}Row row{};hr=rows->GetData(handles[0],binding,&row);rows->ReleaseRows(count,handles,nullptr,nullptr,nullptr);CoTaskMemFree(handles);
            if(SUCCEEDED(hr)&&row.nameStatus==DBSTATUS_S_OK&&row.pathStatus==DBSTATUS_S_OK){Result r;r.title=row.name;r.target=row.path;r.id=L"path:"+fold(r.target);auto slash=r.target.find_last_of(L"\\/");r.detail=slash==r.target.npos?L"Windows indexed file":r.target.substr(0,slash);r.kind=row.attributeStatus==DBSTATUS_S_OK&&(row.attributes&FILE_ATTRIBUTE_DIRECTORY)?Kind::Folder:Kind::File;results.push_back(std::move(r));}}
        accessor->ReleaseAccessor(binding,nullptr);status=L"Windows indexed files. Everything is unavailable.";
    }catch(...){status=L"File search unavailable. Start Everything, or enable Windows Search indexing.";}
    return results;
}
}
