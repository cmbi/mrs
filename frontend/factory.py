from xml.etree import ElementTree

from flask import Flask


def create_app():

    app = Flask("MRS", static_folder='frontend/static', template_folder='frontend/templates')

    mrs_config = ElementTree.parse("/usr/local/etc/mrs/mrs-config.xml").getroot()

    all_databank_names = []
    app.config["databanks"] = []
    for databank in mrs_config.find('databanks'):
        app.config["databanks"].append(databank)
        all_databank_names.append(databank.get("name"))

    from frontend.wsdl import mrs_search
    mrs_search.all_databank_names = all_databank_names
    mrs_search.wsdl_url = "https://mrs.cmbi.umcn.nl/mrsws/search/wsdl"
    mrs_search.endpoint_url = "http://localhost:18090/mrsws/search"

    from frontend.dashboard.views import bp
    app.register_blueprint(bp)

    return app
