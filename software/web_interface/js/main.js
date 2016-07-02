$(document).ready(function()
{
    $('#main_toolbar_status').click(function(event)
    {
        $('#container_configure').addClass('hidden');
        $('#container_configure').removeClass('show');
        $('#container_client_status').addClass('show');
        $('#container_client_status').removeClass('hidden');
        $('#main_toolbar_about').removeClass('active');
        $('#main_toolbar_configure').removeClass('active');
        $('#main_toolbar_status').addClass('active');
        $('#main_toolbar_about').removeClass('active');
    });
    $('#main_toolbar_configure').click(function(event)
    {
        $('#container_configure').addClass('show');
        $('#container_configure').removeClass('hidden');
        $('#container_client_status').addClass('hidden');
        $('#container_client_status').removeClass('show');
        $('#main_toolbar_about').removeClass('active');
        $('#main_toolbar_configure').addClass('active');
        $('#main_toolbar_status').removeClass('active');
        $('#main_toolbar_about').removeClass('active');
    });
    $('#main_toolbar_about').click(function(event)
    {
        $('#main_toolbar_about').removeClass('active');
        $('#main_toolbar_configure').removeClass('active');
        $('#main_toolbar_status').removeClass('active');
        $('#main_toolbar_about').addClass('active');
    });
    $('input[type="checkbox"]').click(function()
    {
        if ($(this).attr("id") == "common_use_authentication")
        {
            $("#common_use_authentication_elements").toggleClass(
                "hidden");
        }
        if ($(this).attr("id") ==
            "client_connect_to_ap_with_specific_bssid")
        {
            $("#client_connect_to_ap_with_specific_bssid_elements").toggleClass(
                "hidden");
        }
        if ($(this).attr("id") ==
            "client_use_custom_mac_address")
        {
            $("#client_use_custom_mac_address_elements").toggleClass(
                "hidden");
        }
    });

    function doAJAXPost(e)
    {
        $.ajax({
            type: 'POST',
            url: '/get_status.cgi',
            data: {'SID': 'sessionID', 'RID': 1, 'DATA': 'optionalDATA'},
            dataType: "json",
            timeout: 16000, // in milliseconds
            success: function(data) {
                // process data here
                console.log('*** success(), data = ' + data);
                console.log(document.cookie);
                setTimeout(doAJAXPost, 1000);
            },
            error: function(request, status, err) {
                console.log('*** error(), request = ' + request);
                console.log('*** error(), status = ' + status);
                console.log('*** error(), err = ' + err);
                console.log(document.cookie);
                setTimeout(doAJAXPost, 1000);
            }
        });
    }

    setTimeout(doAJAXPost, 1000);
});
