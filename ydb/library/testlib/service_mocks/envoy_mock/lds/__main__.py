import grpc
from concurrent import futures
from envoy.service.discovery.v3 import discovery_pb2
from envoy.service.discovery.v3 import ads_pb2_grpc
from envoy.config.listener.v3 import listener_pb2
from envoy.extensions.filters.network.http_connection_manager.v3 import http_connection_manager_pb2
from google.protobuf import any_pb2


class LDSServer(ads_pb2_grpc.AggregatedDiscoveryServiceServicer):
    """Простой LDS сервер"""

    def __init__(self):
        self.version = "1"

    def StreamAggregatedResources(self, request_iterator, context):
        print("🔗 Новое подключение")

        for request in request_iterator:
            print(f"📨 Запрос: {request.type_url}")
            print(f"📋 Ресурсы: {list(request.resource_names)}")

            response = discovery_pb2.DiscoveryResponse()
            response.version_info = self.version
            response.type_url = request.type_url
            response.nonce = str(hash(str(request.resource_names)))

            if "Listener" in request.type_url:
                self._add_listener(response)

            yield response

    def _add_listener(self, response):
        listener = listener_pb2.Listener()
        listener.name = "http_listener"
        listener.address.socket_address.address = "0.0.0.0"
        listener.address.socket_address.port_value = 10001

        # Filter chain
        filter_chain = listener.filter_chains.add()
        http_filter = filter_chain.filters.add()
        http_filter.name = "envoy.filters.network.http_connection_manager"

        # HTTP Connection Manager
        hcm = http_connection_manager_pb2.HttpConnectionManager()
        hcm.stat_prefix = "ingress_http"
        hcm.codec_type = http_connection_manager_pb2.HttpConnectionManager.AUTO

        # Route config
        hcm.route_config.name = "local_route"
        vh = hcm.route_config.virtual_hosts.add()
        vh.name = "local_service"
        vh.domains.append("*")

        # Route to static cluster
        route = vh.routes.add()
        route.match.prefix = "/"
        route.route.cluster = "backend_cluster"

        # Router filter
        router = hcm.http_filters.add()
        router.name = "envoy.filters.http.router"
        router_any = any_pb2.Any()
        router_any.type_url = "type.googleapis.com/envoy.extensions.filters.http.router.v3.Router"
        router_any.value = b""
        router.typed_config.CopyFrom(router_any)

        # Pack HCM
        any_config = any_pb2.Any()
        any_config.Pack(hcm)
        http_filter.typed_config.CopyFrom(any_config)

        # Pack listener
        any_resource = any_pb2.Any()
        any_resource.Pack(listener)
        response.resources.append(any_resource)

        print("✅ Отправлен listener: http_listener на порту 10000")


def main():
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    ads_pb2_grpc.add_AggregatedDiscoveryServiceServicer_to_server(
        LDSServer(), server
    )

    port = 18000
    server.add_insecure_port(f"[::]:{port}")
    server.start()

    print(f"🚀 LDS сервер запущен на порту {port}")

    try:
        server.wait_for_termination()
    except KeyboardInterrupt:
        print("\n🛑 Остановка сервера")


if __name__ == "__main__":
    main()
