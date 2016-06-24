$('#main_toolbar_status').click(function (event) {
	$('#container_configure').addClass('hidden');
	$('#container_configure').removeClass('show');	
	$('#container_client_status').addClass('show');
	$('#container_client_status').removeClass('hidden');
	$('#main_toolbar_about').removeClass('active');
	$('#main_toolbar_configure').removeClass('active');
	$('#main_toolbar_status').addClass('active');
	$('#main_toolbar_about').removeClass('active');
});

$('#main_toolbar_configure').click(function (event) {
	$('#container_configure').addClass('show');
	$('#container_configure').removeClass('hidden');	
	$('#container_client_status').addClass('hidden');
	$('#container_client_status').removeClass('show');
	$('#main_toolbar_about').removeClass('active');
	$('#main_toolbar_configure').addClass('active');
	$('#main_toolbar_status').removeClass('active');
	$('#main_toolbar_about').removeClass('active');
});

$('#main_toolbar_about').click(function (event) {
	$('#main_toolbar_about').removeClass('active');
	$('#main_toolbar_configure').removeClass('active');
	$('#main_toolbar_status').removeClass('active');
	$('#main_toolbar_about').addClass('active');
});
