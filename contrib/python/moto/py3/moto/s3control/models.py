from collections import defaultdict
from datetime import datetime
from moto.core import BaseBackend, BaseModel
from moto.core.utils import BackendDict
from moto.moto_api._internal import mock_random
from moto.s3.exceptions import (
    WrongPublicAccessBlockAccountIdError,
    NoSuchPublicAccessBlockConfiguration,
    InvalidPublicAccessBlockConfiguration,
)
from moto.s3.models import PublicAccessBlock

from .exceptions import AccessPointNotFound, AccessPointPolicyNotFound


class AccessPoint(BaseModel):
    def __init__(
        self,
        account_id,
        name,
        bucket,
        vpc_configuration,
        public_access_block_configuration,
    ):
        self.name = name
        self.alias = f"{name}-{mock_random.get_random_hex(34)}-s3alias"
        self.bucket = bucket
        self.created = datetime.now().strftime("%Y-%m-%dT%H:%M:%S.%f")
        self.arn = f"arn:aws:s3:us-east-1:{account_id}:accesspoint/{name}"
        self.policy = None
        self.network_origin = "VPC" if vpc_configuration else "Internet"
        self.vpc_id = (vpc_configuration or {}).get("VpcId")
        pubc = public_access_block_configuration or {}
        self.pubc = {
            "BlockPublicAcls": pubc.get("BlockPublicAcls", "true"),
            "IgnorePublicAcls": pubc.get("IgnorePublicAcls", "true"),
            "BlockPublicPolicy": pubc.get("BlockPublicPolicy", "true"),
            "RestrictPublicBuckets": pubc.get("RestrictPublicBuckets", "true"),
        }

    def delete_policy(self):
        self.policy = None

    def set_policy(self, policy):
        self.policy = policy

    def has_policy(self):
        return self.policy is not None


class S3ControlBackend(BaseBackend):
    def __init__(self, region_name, account_id):
        super().__init__(region_name, account_id)
        self.public_access_block = None
        self.access_points = defaultdict(dict)

    def get_public_access_block(self, account_id):
        # The account ID should equal the account id that is set for Moto:
        if account_id != self.account_id:
            raise WrongPublicAccessBlockAccountIdError()

        if not self.public_access_block:
            raise NoSuchPublicAccessBlockConfiguration()

        return self.public_access_block

    def delete_public_access_block(self, account_id):
        # The account ID should equal the account id that is set for Moto:
        if account_id != self.account_id:
            raise WrongPublicAccessBlockAccountIdError()

        self.public_access_block = None

    def put_public_access_block(self, account_id, pub_block_config):
        # The account ID should equal the account id that is set for Moto:
        if account_id != self.account_id:
            raise WrongPublicAccessBlockAccountIdError()

        if not pub_block_config:
            raise InvalidPublicAccessBlockConfiguration()

        self.public_access_block = PublicAccessBlock(
            pub_block_config.get("BlockPublicAcls"),
            pub_block_config.get("IgnorePublicAcls"),
            pub_block_config.get("BlockPublicPolicy"),
            pub_block_config.get("RestrictPublicBuckets"),
        )

    def create_access_point(
        self,
        account_id,
        name,
        bucket,
        vpc_configuration,
        public_access_block_configuration,
    ):
        access_point = AccessPoint(
            account_id,
            name,
            bucket,
            vpc_configuration,
            public_access_block_configuration,
        )
        self.access_points[account_id][name] = access_point
        return access_point

    def delete_access_point(self, account_id, name):
        self.access_points[account_id].pop(name, None)

    def get_access_point(self, account_id, name):
        if name not in self.access_points[account_id]:
            raise AccessPointNotFound(name)
        return self.access_points[account_id][name]

    def create_access_point_policy(self, account_id, name, policy):
        access_point = self.get_access_point(account_id, name)
        access_point.set_policy(policy)

    def get_access_point_policy(self, account_id, name):
        access_point = self.get_access_point(account_id, name)
        if access_point.has_policy():
            return access_point.policy
        raise AccessPointPolicyNotFound(name)

    def delete_access_point_policy(self, account_id, name):
        access_point = self.get_access_point(account_id, name)
        access_point.delete_policy()

    def get_access_point_policy_status(self, account_id, name):
        """
        We assume the policy status is always public
        """
        self.get_access_point_policy(account_id, name)
        return True


s3control_backends = BackendDict(
    S3ControlBackend,
    "s3control",
    use_boto3_regions=False,
    additional_regions=["global"],
)
