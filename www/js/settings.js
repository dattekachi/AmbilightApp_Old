var storedLang;
var availLang = ['vi', 'en'];
var availLangText = ['Tiếng Việt', 'English'];

//$.i18n.debug = true;

//Change Password
function changePassword(){
	showInfoDialog('changePassword', $.i18n('InfoDialog_changePassword_title'));

	// fill default pw if default is set
	if(window.defaultPasswordIsSet)
		$('#oldPw').val('ambilightapp')

	$('#id_btn_ok').off().on('click',function() {
		var oldPw = $('#oldPw').val();
		var newPw = $('#newPw').val();

		requestChangePassword(oldPw, newPw);

		$('#new_modal_dialog').modal('toggle');
	});



	$('#newPw, #oldPw').off().on('input',function(e) {
		($('#oldPw').val().length >= 8 && $('#newPw').val().length >= 8) && !window.readOnlyMode ? $('#id_btn_ok').attr('disabled', false) : $('#id_btn_ok').attr('disabled', true);
	});
}

$(document).ready( function() {

	//i18n
	function initTrans(lc){
		if (lc == 'auto')
		{
			$.i18n().locale = 'vi';
			$.i18n().load().done(
			function() {
				performTranslation();
			});
		}
		else
		{
			$.i18n().locale = lc;
			$.i18n().load( "i18n", lc ).done(
			function() {
				performTranslation();
			});
		}
	}

	if (storageComp())
	{
		storedLang = getStorage("langcode");
		if (storedLang == null)
		{
			setStorage("langcode", 'vi');
			storedLang = 'vi';
			initTrans(storedLang);
		}
		else
		{
			initTrans(storedLang);
		}
	}
	else
	{
		showInfoDialog('warning', "Can't store settings", "Your browser doesn't support localStorage. You can't save a specific language setting (fallback to 'auto detection') and access level (fallback to 'default'). Some wizards may be hidden. You could still use the webinterface without further issues");
		initTrans('vi');
		storedLang = 'vi';		
		$('#btn_setlang').attr("disabled", true);
	}

	initLanguageSelection();

	// change pw btn
	$('#btn_changePassword').off().on('click',function() {
		changePassword();
	});

	//Lock Ui
	$('#btn_lock_ui').off().on('click',function() {
		requestLogout();
		removeStorage('loginToken', true);
		location.replace('/');
	});	
});

function compareAmbilightAppVersion(compareA, compareB)
{
	if (compareA.length == 0 || compareB.length == 0)
	{
		console.log(`Invalid length: A:${compareA.length} B:${compareB.length}`);
		return true;
	}
	
	if (isNaN(compareA[0]))
		compareA = compareA.substring(1);
	if (isNaN(compareB[0]))
		compareB = compareB.substring(1);
		
	if ((compareA.indexOf('alpha') >= 0) &&
		(compareB.indexOf('alpha') < 0))
		{
			return false;
		}
		
	if ((compareB.indexOf('alpha') >= 0) &&
		(compareA.indexOf('alpha') < 0))
		{
			return true;
		}		
		
		
	var valueA = compareA.split('.');
	var valueB = compareB.split('.');
	
	if (valueA.length < 4 || valueB.length < 4)
	{
		console.log(`Invalid length: A:${valueA.length} B:${valueB.length}`);
		return true;
	}
	
	var finalA = "";
	for (var i = 0; i < valueA[3].length; i++)
		if (!isNaN(valueA[3][i]))
			finalA = finalA.concat(valueA[3][i]);
	valueA[3] = finalA;
	
	var finalB = "";
	for (var i = 0; i < valueB[3].length; i++)
		if (!isNaN(valueB[3][i]))
			finalB = finalB.concat(valueB[3][i]);
	valueB[3] = finalB;

	if ((compareA.indexOf('beta') >= 0 || compareA.indexOf('alpha') >= 0) &&
		(compareB.indexOf('beta') < 0 && compareB.indexOf('alpha') < 0))
	{
		if (Number(valueA[0]) <= Number(valueB[0]))
			return false;
	}

	if ((compareB.indexOf('beta') >= 0 || compareB.indexOf('alpha') >= 0) &&
		(compareA.indexOf('beta') < 0 && compareA.indexOf('alpha') < 0))
	{
		if (Number(valueA[0]) >= Number(valueB[0]))
			return true;
	}
	
	for (var i = 0; i < 4; i++)
	{
		if (Number(valueA[i]) > Number(valueB[i]))
		{
			return true;
		}
		else if (Number(valueA[i]) < Number(valueB[i]))
		{
			return false;
		}
	}
	
	return false;
}
