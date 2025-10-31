// OcmMapEngine.cpp
#include "OcmMapEngine.hpp"
#include "ThreadPool.hpp"
#include <olp/clientmap/datastore/DataStoreClient.h>
#include <olp/clientmap/datastore/DataStoreServer.h>
#include <olp/core/client/OlpClientSettingsFactory.h>
#include <olp/clientmap/datastore/DataStoreServerBuilder.h>
#include <olp/authentication/TokenProvider.h>
#include <olp/core/logging/Log.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <stdexcept>

using namespace std;
using namespace olp;
using namespace olp::clientmap::datastore;

namespace ning {
namespace maps {
namespace ocm {

class OcmMapEngine::OcmMapEngineImpl : public enable_shared_from_this<OcmMapEngineImpl> {
public:
    explicit OcmMapEngineImpl(const Settings& settings)
        : m_settings(settings) {}

    ~OcmMapEngineImpl() noexcept {

    }

    OcmMapEngineImpl(const OcmMapEngineImpl&) = delete;
    OcmMapEngineImpl& operator=(const OcmMapEngineImpl&) = delete;



    Response<datastore::TileLoadResult>  FetchTileAsync(
        const geo::TileKey& tileKey,
        const TileRequest::Layers& layers) 
    {
        return FetchTileInternal(tileKey, layers);
    }

private:
    boost::optional<olp::authentication::AuthenticationCredentials>
    GetAuthenticationCredentials() {
        if (!m_settings.access_key_id.empty() && !m_settings.access_key_secret.empty()) {
            return olp::authentication::AuthenticationCredentials(
                m_settings.access_key_id, m_settings.access_key_secret);
        }
        return olp::authentication::AuthenticationCredentials::ReadFromFile(m_settings.path_to_credentials_file); 
    }


    /// Get latest version of the catalog
    boost::optional< int64_t >
    GetLatestVersion( const std::shared_ptr< olp::clientmap::datastore::DataStoreServer >& server,
                    const CatalogHandle 	catalog_handle)
    {


        std::promise< datastore::Response< int64_t > > version_promise;
        server->GetLatestVersion( catalog_handle,
                                [&]( datastore::Response< int64_t > response ) {
                                    version_promise.set_value( std::move( response ) );
                                } );

        auto version_future = version_promise.get_future( );
        const auto version_response = version_future.get( );
        if ( !version_response )
        {
            return boost::none;
        }

        return version_response.GetResult( );
    }

    Response<datastore::TileLoadResult>  FetchTileInternal(
        const geo::TileKey& tileKey,
        const TileRequest::Layers& layers) 
    {

        auto credentials = GetAuthenticationCredentials();
        if (!credentials) {
            OLP_SDK_LOG_ERROR("OcmMapEngineImpl", "No valid authentication credentials found.");
            //return false;
        }

        olp::cache::CacheSettings cache_settings;
        cache_settings.disk_path_mutable = m_settings.cache_folder+"/MutableCache";
        cache_settings.disk_path_protected = m_settings.cache_folder+"/ProtectCache";

        olp::client::RetrySettings retry_settings;
        retry_settings.transfer_timeout = std::chrono::seconds(120);
        retry_settings.timeout =  180;
        retry_settings.max_attempts = 6;


        auto task_scheduler_unique = olp::client::OlpClientSettingsFactory::CreateDefaultTaskScheduler(4u);
        auto task_scheduler = std::shared_ptr<olp::thread::TaskScheduler>(std::move(task_scheduler_unique));

        const auto server  = DataStoreServerBuilder()
                       .WithCustomCacheSettings(cache_settings)
                       .WithCustomTaskScheduler(task_scheduler)
                       .WithCustomRetrySettings(retry_settings)
                       .Build();

        server->Init();
        server->SetOnline(true);

                const auto version_response = server -> GetAvailableVersion(
        m_settings.catalog_hrn, olp::cache::DefaultCache::CacheType::kProtected );

        datastore::DataStoreClient client(server, datastore::DataStoreClientSettings{64u} );
       // m_layer_client = make_shared<DataStoreClient>(m_server, DataStoreClientSettings{64u});
        auto add_server_catalog_response =
            datastore::AddCatalog(*server, m_settings.catalog_hrn,
                                  m_settings.catalog_version, credentials);
        if (!add_server_catalog_response) {
            OLP_SDK_LOG_ERROR_F("OcmMapEngineImpl",
                                "Failed to add catalog to server: %s",
                                ToString(add_server_catalog_response.GetError()).c_str());
           // return false;
        }

       auto catalogVersion = m_settings.catalog_version;

       if(catalogVersion == 0)
       {
          catalogVersion = GetLatestVersion(server, add_server_catalog_response.GetResult()).value_or(0);
        }
        
       if(catalogVersion == 0)
       {
        catalogVersion = 196;
       }

        datastore::AddCatalog(*server, m_settings.catalog_hrn,
                                  catalogVersion, credentials);
       
        auto catalog_handle = client.AddCatalog(
            m_settings.catalog_hrn, ClientCatalogSettings{catalogVersion});
        if (!catalog_handle) {
            OLP_SDK_LOG_ERROR_F("OcmMapEngineImpl",
                                "Failed to add catalog to client: %s",
                                ToString(catalog_handle.GetError()).c_str());
           // return false;
        }
        std::promise< Response<datastore::TileLoadResult>  > load_promise;

        auto callback = [&]( const datastore::Response< datastore::TileLoadResult >& response ) {
            load_promise.set_value(response);
        };

        auto load_request = LoadTileRequest().WithTileKey(tileKey).WithLayers(layers);
        client.Load(load_request).Detach( std::move( callback ) );

        return load_promise.get_future( ).get( );
    }

private:
    Settings m_settings;
};

// OcmMapEngine implementation
OcmMapEngine::OcmMapEngine(const Settings& settings)
    : m_impl(make_shared<OcmMapEngineImpl>(settings)) {}

OcmMapEngine::~OcmMapEngine() = default;


Response<datastore::TileLoadResult>  OcmMapEngine::FetchTileAsync(
    const geo::TileKey& tileKey,
    const datastore::TileRequest::Layers& layers) 
{
    return m_impl->FetchTileAsync(tileKey, layers);
                 
}

} // namespace ocm
} // namespace maps
} // namespace ning