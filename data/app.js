var app = angular.module('singleApp', []);

app.controller('singleCtrl', ['$scope', '$http', function ($scope, $http) {

    function downloadData() {
        $http.get('data.json?v=' + Math.random())
        .then(function (response) {
            $scope.data = response.data;
        }, function (response) {
            $scope.error = response.data.error;
        });
    }

    downloadData();

    $scope.save = function() {
        $scope.error = null;
        var data = new FormData();
        data.append('data', JSON.stringify($scope.data));

        $http.post('/save', data, {transformRequest: angular.identity, headers: {'Content-Type': undefined}})
        .then(function (response) {}, function (response) {
            $scope.error = response.data ? response.data.error: "Ошибка при сохранении";
        });
    };

}]);


app.directive('myTemplate', function($templateCache, $compile) {
    return {
        restrict: 'A',
        scope:{
            template : "@",
            item : "=",
        },
        link: function(scope,element) {
            var template = $templateCache.get(scope.template);
            scope.item = scope.item;
            element.append($compile(template)(scope));
        }
    }
});