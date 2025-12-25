import grpc
from concurrent import futures
from envoy.service.discovery.v3 import discovery_pb2
from envoy.service.discovery.v3 import ads_pb2_grpc
from envoy.config.endpoint.v3 import endpoint_pb2
from envoy.config.listener.v3 import listener_pb2
from envoy.config.core.v3 import base_pb2
from envoy.extensions.filters.network.http_connection_manager.v3 import http_connection_manager_pb2
from google.protobuf import any_pb2


class XDSServer(ads_pb2_grpc.AggregatedDiscoveryServiceServicer):

    def __init__(self):
        self.version = "1"
        self.sent_listeners = False
        self.sent_endpoints = {}

    def StreamAggregatedResources(self, request_iterator, context):
        print("=" * 50)
        print("🔗 Envoy подключился к xDS серверу")
        print("=" * 50)

        for request in request_iterator:
            print(f"\n📥 Запрос:")
            print(f"   Type: {request.type_url}")
            print(f"   Version: '{request.version_info}'")
            print(f"   Nonce: '{request.response_nonce}'")
            print(f"   Resources: {list(request.resource_names)}")

            # Это ACK на предыдущий ответ - не отправляем новый ответ
            if request.version_info == self.version:
                print("   ✓ ACK получен, пропускаем")
                continue

            # Это NACK (ошибка) - проверяем error_detail
            if request.error_detail.message:
                print(f"   ✗ NACK: {request.error_detail.message}")
                continue

            response = discovery_pb2.DiscoveryResponse()
            response.version_info = self.version
            response.type_url = request.type_url
            response.nonce = f"nonce-{request.type_url.split('.')[-1]}"

            # LDS запрос
            if "Listener" in request.type_url:
                print("\n📤 Отправляем LDS ответ")
                self._add_listener(response)

            # EDS запрос
            elif "ClusterLoadAssignment" in request.type_url:
                print("\n📤 Отправляем EDS ответ")
                self._add_endpoints(request, response)

            yield response

    def _add_listener(self, response):
        listener = listener_pb2.Listener()
        listener.name = "http_listener"
        listener.address.socket_address.address = "0.0.0.0"
        listener.address.socket_address.port_value = 10000

        filter_chain = listener.filter_chains.add()
        http_filter = filter_chain.filters.add()
        http_filter.name = "envoy.filters.network.http_connection_manager"

        hcm = http_connection_manager_pb2.HttpConnectionManager()
        hcm.stat_prefix = "ingress_http"
        hcm.codec_type = hcm.AUTO

        hcm.route_config.name = "local_route"
        vh = hcm.route_config.virtual_hosts.add()
        vh.name = "local_service"
        vh.domains.append("*")

        route = vh.routes.add()
        route.match.prefix = "/"
        route.route.cluster = "testing"

        router = hcm.http_filters.add()
        router.name = "envoy.filters.http.router"
        router_any = any_pb2.Any()
        router_any.type_url = "type.googleapis.com/envoy.extensions.filters.http.router.v3.Router"
        router_any.value = b""
        router.typed_config.CopyFrom(router_any)

        any_config = any_pb2.Any()
        any_config.Pack(hcm)
        http_filter.typed_config.CopyFrom(any_config)

        any_resource = any_pb2.Any()
        any_resource.Pack(listener)
        response.resources.append(any_resource)

        print("   - Listener: http_listener:10000 → testing")

    def _add_endpoints(self, request, response):
        for cluster_name in request.resource_names:
            cla = endpoint_pb2.ClusterLoadAssignment()
            cla.cluster_name = cluster_name

            locality = cla.endpoints.add()

            for port in [8000, 8001]:
                ep = locality.lb_endpoints.add()
                ep.endpoint.address.socket_address.address = "127.0.0.1"
                ep.endpoint.address.socket_address.port_value = port
                ep.health_status = base_pb2.HEALTHY

            any_resource = any_pb2.Any()
            any_resource.Pack(cla)
            response.resources.append(any_resource)

            print(f"   - {cluster_name}: [127.0.0.1:8000, 127.0.0.1:8001]")


def main():
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    ads_pb2_grpc.add_AggregatedDiscoveryServiceServicer_to_server(
        XDSServer(), server
    )

    server.add_insecure_port("[::]:18000")
    server.start()

    print("🚀 xDS сервер запущен на порту 18000")

    try:
        server.wait_for_termination()
    except KeyboardInterrupt:
        print("\n🛑 Остановка")


if __name__ == "__main__":
    main()
