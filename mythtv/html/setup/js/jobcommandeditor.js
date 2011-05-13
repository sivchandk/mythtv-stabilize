
function initJobCommandEditor() {
    $("#jobcommands").jqGrid({
        url: '/JobQueue/GetJobCommandList',
        datatype: 'json',
        colNames: ['Command ID', 'Command Type', 'Name', 'SubName', 'Short Description',
                   'Long Description', 'Path', 'Arguments', 'Default', 'Needs File Access',
                   'Is CPU Intense', 'Is Disk Intense', 'Is Sequence'],
        colModel: [
            {name:'CmdId',      editable:false, width: 120, sorttype:"int",  hidden:true, jsonmap:'CmdId'},
            {name:'Type',       editable:false, width: 120, sorttype:"text",              jsonmap:'Type'},
            {name:'Name',       editable:false, width: 200, sorttype:"text",              jsonmap:'Name'},
            {name:'SubName',    editable:false, width: 200, sortable:false,               jsonmap:'SubName'},
            {name:'ShortDesc',  editable:false, width: 0,   sortable:false,  hidden:true, jsonmap:'ShortDesc'},
            {name:'LongDesc',   editable:false, width: 0,   sortable:false,  hidden:true, jsonmap:'LongDesc'},
            {name:'Path',       editable:true,  width: 0,   sortable:false,  hidden:true, jsonmap:'Path'},
            {name:'Args',       editable:true,  width: 0,   sortable:false,  hidden:true, jsonmap:'Args'},
            {name:'isDefualt',  editable:true,  width: 0,   sortable:false,  hidden:true, jsonmap:'Default'},
            {name:'NeedsFile',  editable:true,  width: 0,   sortable:false,  hidden:true, jsonmap:'NeedsFile'},
            {name:'CPUIntense', editable:true,  width: 0,   sortable:false,  hidden:true, jsonmap:'CPUIntense'},
            {name:'DiskIntense',editable:true,  width: 0,   sortable:false,  hidden:true, jsonmap:'DiskIntense'},
            {name:'Sequence',   editable:false, width: 0,   sortable:false,  hidden:true, jsonmap:'Sequence'}
        ],
        jsonReader: {
            root:"JobCommandList.JobCommands",
            page:"JobCommandList.CurrentPage",
            total:"JobCommandList.TotalPages",
            records:"JobCommandList.TotalAvailable",
            cell:"JobCommands",
            id:"CmdId",
            repeatitems:false
        },
        rowNum:20,
        multiselect: true,
        rowList:[10,20,30,50],
        pager: '#pager',
        defaultSearch: 'cn',
        ignoreCase: true,
        sortname: 'Type',
        viewrecords: true,
        sortorder: 'desc',
        loadonce: true,
        autoWidth: true,
        width: 820,
        height: 442
    });

    jQuery("#jobcommands").jqGrid('filterToolbar',{ searchOnEnter: false, defaultSearch: "cn" });

    $(window).bind('resize', function() {
        $("#jobcommands").setGridWidth($(window).width() - 223);
    }).trigger('resize');

    $("#jobcommands").setGridParam({sortname:'CmdId', sortorder:'asc'}).trigger('reloadGrid');

    /* Initialize the Popup Menu */

    $("#jobcommands").contextMenu('jobcommandmenu', {
            bindings: {
                'editopt': function(t) {
                    editJobCommand();
                },
                'del': function(t) {
                    promptToDeleteJobCommand();
                }
            },
            onContextMenu : function(event, menu) {
                var rowId = $(event.target).parent("tr").attr("id")
                var grid = $("#jobcommands");
                if (rowId != $('#jobcommands').getGridParam('selrow'))
                    grid.setSelection(rowId);
                return true;
            }
    });
}

function editJobCommand() {
    var rowNum = $('#jobcommands').getGridParam('selrow');
    var rowArr = $('#jobcommands').getGridParam('selarrrow');
    if (rowNum != null) {
        if (rowArr.length == 1) {
            editSelectedJobCommand();
        } else {
            editMultiJobCommand();
        }
    }
}

function editSelectedJobCommand() {
    loadEditWindow("/setup/jobcommandeditor-detail.html");
    var row = $('#jobcommands').getGridParam('selrow');
    var rowdata = $("#jobcommands").jqGrid('getRowData', row);

// information printout
    $("#commanddetail-info-cmdid").html(rowdata.CmdId);
    $("#commanddetail-info-type").html(rowdata.Type);
    $("#commanddetail-info-name").html(rowdata.Name);
    $("#commanddetail-info-subname").html(rowdata.SubName);
    $("#commanddetail-info-sdesc").html(rowdata.ShortDesc);
    $("#commanddetail-info-ldesc").html(rowdata.LongDesc);

// command editor
    $("#commanddetail-setting-path").val(rowdata.Path);
    $("#commanddetail-setting-args").val(rowdata.Args);

    if (rowdata.isDefault == "Yes")
        $("#commanddetail-setting-default").attr('checked', "true");
    if (rowdata.NeedsFile == "Yes")
        $("#commanddetail-setting-needsfile").attr('checked', "true");
    if (rowdata.CPUIntense == "Yes")
        $("#commanddetail-setting-cpuintense").attr('checked', "true");
    if (rowdata.DiskIntense == "Yes")
        $("#commanddetail-setting-diskintense").attr('checked', "true");

// host editor
    $("#commanddetail-host-select").html("Host: <select id='commanddetail-host' onChange='javascript:changeJobHost()'>"+getJobHostList()+"</select>");

    $("#edit").dialog({
        modal: true,
        width: 800,
        height: 620,
        'title': 'Edit Command',
        closeOnEscape: false,
        buttons: {
           'Save': function() { saveJobCommandEdits(); },
           'Cancel': function() { $(this).dialog('close'); }
    }});

    $("#jobcommandsettings").accordion({ active: 1 });
    $("#edit").dialog("open");
}

function editMultiJobCommand() {
    loadEditWindow("/setup/jobcommandeditor-multidetail.html");
    var rows = $('#jobcommands').getGridParam('selarrrow');

    $('#edit').dialog({
        modal: true,
        width: 800,
        height: 620,
        'title': 'Edit Multiple Commands',
        closeOnEscape: false,
        buttons: {
           'Save': function() { showConfirm('Editing multiple job commands at once should be taken with great care. Dow you want to continue? This cannot be undone.', saveMultiJobCommandEdits); },
           'Cancel': function() { $(this).dialog('close'); }
    }});

    $('#multicommandsettings').accordion();
}

function changeJobHost() {

}

function getJobHostList() {

}

function saveJobCommandEdits() {

}

function addJobHost() {

}

function saveJobHost() {

}

function deleteJobHost() {

}

function addJobCommand() {
    loadEditWindow('/setup/jobcommandeditor-create.html');

    $('#edit').dialog({
        modal: true,
        width: 800,
        height: 620,
        'title': 'Create new command',
        closeOnEscape: false,
        buttons: {
           'Save': function() { saveNewJobCommand(); },
           'Cancel': function() { $(this).dialog('close'); }
    }});

    $('#jobcommandcreation').accordion();
}

function saveNewJobCommand() {
    var type = $('#commanddetail-setting-type').val();
    var name = $('#commanddetail-setting-name').val();
    var subname =  $('#commanddetail-setting-subname').val();
    var shortdesc = $('#commanddetail-setting-sdesc').val();
    var longdesc = $('#commanddetail-setting-ldesc').val();
    var path = $('#commanddetail-setting-path').val();
    var args = $('#commanddetail-setting-args').val();
    var isdefault = $('#commanddetail-setting-default').val();
    var needsfile = $('#commanddetail-setting-needsfile').val();
    var cpuintense = $('#commanddetail-setting-cpuintense').val();
    var diskintense = $('#commanddetail-setting-diskintense').val();

    $.post("/JobQueue/CreateJobCommand",
        {'Type': type, 'Name': name, 'SubName': subname, 'ShortDesc': shortdesc,
         'LongDesc': longdesc, 'Path': path, 'Args': args, 'Default': isdefault,
         'NeedsFile': needsfile, 'CPUIntense': cpuintense, 'DiskIntense': diskintense,
         'Sequence': false},
        function(data) {
            if (data.bool == "true") {
                setStatusMessage("Command addition successful!");
                $("#edit").dialog('close');
            }
            else
                setErrorMessage("Command addition failed!");
        }, "json");
         
    $('#jobcommands').trigger('reloadGrid');
}

function saveJobCommandEdits() {
    var cmdid = $('#commanddetail-info-cmdid').html();
    var type = $('#commanddetail-info-type').html();
    var name = $('#commanddetail-info-name').html();
    var subname =  $('#commanddetail-info-subname').html();
    var shortdesc = $('#commanddetail-info-sdesc').html();
    var longdesc = $('#commanddetail-info-ldesc').html();
    var path = $('#commanddetail-setting-path').val();
    var args = $('#commanddetail-setting-args').val();
    var isdefault = $('#commanddetail-setting-default').val();
    var needsfile = $('#commanddetail-setting-needsfile').val();
    var cpuintense = $('#commanddetail-setting-cpuintense').val();
    var diskintense = $('#commanddetail-setting-diskintense').val();

    $.post("/JobQueue/UpdateJobCommand",
        {'CmdId': cmdid, 'Type': type, 'Name': name, 'SubName': subname,
         'ShortDesc': shortdesc, 'LongDesc': longdesc, 'Path': path,
         'Args': args, 'Default': isdefault, 'NeedsFile': needsfile,
         'CPUIntense': cpuintense, 'DiskIntense': diskintense,
         'Sequence': false},
        function(data) {
            if (data.bool == "true") {
                setStatusMessage("Command update successful!");
                $("#edit").dialog('close');
            }
            else
                setErrorMessage("Command update failed!");
        }, "json");
         
    $('#jobcommands').trigger('reloadGrid');
}

function saveMultiJobCommandEdits() {

}

function promptToDeleteJobCommand() {
    var message = "Are you sure you want to delete these commands? This cannot be undone.";
    var rowNum = $('#jobcommands').getGridParam('selrow');
    var rowArr = $('#jobcommands').getGridParam('selarrrow');
    if (rowNum != null) {
        if (rowArr.length == 1) {
            message = "Are you sure you want to delete this command? This cannot be undone.";
        }
        showConfirm(message, deleteSelectedJobCommand);
    }
}

function deleteSelectedJobCommand() {
    var rowArray = $('#jobcommands').jqGrid('getGridParam', 'selarrrow');
    if (rowArray.length > 0) {
        var len = rowArray.length;
        for (var i=0; i < len; i++) {
            var cmdid = rowArray[i];
            if ($('#jobcommands').jqGrid('delRowData', cmdid)) {
                $.post('/JobQueue/DeleteJobCommand',
                    { CmdId: cmdid },
                    function(data) {
                        if (data.bool == "true") {
                            setStatusMessage("Channel deleted successfully!");
                        }
                        else
                            setErrorMessage("Channel delete failed!");
                    }, "json");
            }
        }
        $('#jobcommands').trigger('reloadGrid');
    }
}

initJobCommandEditor();
