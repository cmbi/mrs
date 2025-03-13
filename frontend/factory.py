from xml.etree import ElementTree

from flask import Flask


def create_app():

    app = Flask("MRS", static_folder='frontend/static', template_folder='frontend/templates')
    app.config["mrs_executable"] = "/usr/local/bin/mrs"

    mrs_config = ElementTree.parse("/usr/local/etc/mrs/mrs-config.xml").getroot()

    alias_databank_names = set([])
    all_databank_names = []
    databank_formats = {}
    databank_parsers = {}
    app.config["databanks"] = []
    for databank in mrs_config.find('databanks'):
        if databank.get("enabled") != "true":
            continue

        if "aliases" in databank:
            for alias in databank.get("aliases"):
                alias_name = alias.get("name")
                alias_databank_names.add(alias_name)

        app.config["databanks"].append(databank)
        all_databank_names.append(databank.get("id"))

        if databank.get("format") is not None:
            databank_formats[databank.get("id")] = databank.get("format")

        if databank.get("parser") is not None:
            databank_parsers[databank.get("id")] = databank.get("parser")

    app.config["databank_parsers"] = databank_parsers

    app.config["directories"] = []
    for directory in mrs_config.find('directories'):
        app.config["directories"].append(directory)

    app.jinja_env.globals.update(all_databank_names=all_databank_names)
    app.jinja_env.globals.update(alias_databank_names=alias_databank_names)
    app.jinja_env.globals.update(databank_formats=databank_formats)

    #from frontend.wsdl import mrs_search
    #mrs_search.all_databank_names = all_databank_names
    #mrs_search.wsdl_url = "https://mrs.cmbi.umcn.nl/mrsws/search/wsdl"
    #mrs_search.endpoint_url = "http://localhost:18090/mrsws/search"

    from frontend.dashboard.views import bp
    app.register_blueprint(bp)

    return app
