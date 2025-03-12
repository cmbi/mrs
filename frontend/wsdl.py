from typing import List, Optional
import re

from zeep import Client
from zeep.helpers import serialize_object


def soap_call(wsdl_url: str, endpoint_url: str, method_name: str, *args):

    client = Client(wsdl_url)
    service = client.create_service(client.service._binding.name, endpoint_url)

    response = getattr(service, method_name)(*args)

    return serialize_object(response)


NONBOOLEAN_SEARCH_PATTERN = re.compile(r"[A-Za-z0-9_ ]+")


class MrsSearch:
    def __init__(self,
       all_databank_names: Optional[List[str]] = None,
       wsdl_url: Optional[str] = None,
       endpoint_url: Optional[str] = None,
    ):

        self.all_databank_names = all_databank_names
        self.wsdl_url = wsdl_url
        self.endpoint_url = endpoint_url

    @staticmethod
    def is_boolean(q: str):
        m = NONBOOLEAN_SEARCH_PATTERN.match(q)

        return m is None

    def search(self, db: str, q: str, result_offset: int, max_results: int):

        if self.is_boolean(q):

            r = soap_call(self.wsdl_url, self.endpoint_url, "FindBoolean", db, q, result_offset, max_results)
        else:
            r = soap_call(self.wsdl_url, self.endpoint_url, "Find", db, q, True, "", result_offset, max_results)

        return r

mrs_search = MrsSearch()
