import grpc
from concurrent import futures
# from ydb.library.testlib.service_mocks.envoy_mock.proto import eds_pb2_grpc
# from ydb.library.testlib.service_mocks.envoy_mock.proto import discovery_pb2
# from ydb.library.testlib.service_mocks.envoy_mock.proto import endpoint_pb2
# from ydb.library.testlib.service_mocks.envoy_mock.proto import base_pb2
from envoy.service.endpoint.v3 import eds_pb2_grpc
from envoy.service.discovery.v3 import discovery_pb2
from envoy.config.endpoint.v3 import endpoint_pb2
from envoy.config.core.v3 import address_pb2, base_pb2
from google.protobuf import any_pb2


class SimpleEDSServicer(eds_pb2_grpc.EndpointDiscoveryServiceServicer):
    """Упрощенный EDS сервис"""

    def StreamEndpoints(self, request_iterator, context):
        """Streaming EDS endpoint"""
        for request in request_iterator:
            response = discovery_pb2.DiscoveryResponse()
            response.version_info = "v1"
            response.type_url = "type.googleapis.com/envoy.config.endpoint.v3.ClusterLoadAssignment"

            # Создаем ClusterLoadAssignment
            for cluster_name in request.resource_names:
                cla = endpoint_pb2.ClusterLoadAssignment(
                    cluster_name=cluster_name
                )

                # Добавляем endpoints
                locality = cla.endpoints.add()
                endpoint = locality.lb_endpoints.add()
                endpoint.endpoint.address.socket_address.address = "192.168.1.10"
                endpoint.endpoint.address.socket_address.port_value = 8080
                endpoint.health_status = base_pb2.HEALTHY

                # Упаковываем в response
                resource = any_pb2.Any()
                resource.Pack(cla)
                response.resources.append(resource)

            yield response


def serve():
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    eds_pb2_grpc.add_EndpointDiscoveryServiceServicer_to_server(
        SimpleEDSServicer(), server
    )
    server.add_insecure_port('[::]:18000')
    server.start()
    print("Simple EDS Server started on port 18000")
    server.wait_for_termination()


def main():
    print("+++")
    serve()
