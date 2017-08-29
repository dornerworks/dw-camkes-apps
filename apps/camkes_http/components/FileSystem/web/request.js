/* LICENSE: http://git.savannah.nongnu.org/cgit/lwip.git/tree/COPYING */

/* Make request to server */
function make_request(url)
{
    var http_request = false;

    data_received = true;

    // Mozilla, Safari,...
    if (window.XMLHttpRequest)
    {
        http_request = new XMLHttpRequest();
        if (http_request.overrideMimeType)
        {
            http_request.overrideMimeType('text/xml');
        }
    }
    // IE
    else if (window.ActiveXObject)
    {
        try
        {
            http_request = new ActiveXObject("Msxml2.XMLHTTP");
        }
        catch (e)
        {
            try
            {
                http_request = new ActiveXObject("Microsoft.XMLHTTP");
            }
            catch (e)
            {
            }
        }
    }

    if (!http_request)
    {
        alert('Giving up :( Cannot create an XMLHTTP instance');
        return false;
    }

    http_request.onreadystatechange = function() { alertContents(http_request); };
    http_request.open('GET', url, true);
    http_request.send(null);
}

function alertContents(http_request)
{
    var HTTPStatus_OK = 200;
    var XMLHTTPRequest_Done = 4;

    if (http_request.readyState == XMLHTTPRequest_Done)
    {
        if (http_request.status == HTTPStatus_OK)
        {
            parse_vars(http_request.responseText);
        }
        data_received = false;
    }
}
