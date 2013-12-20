<?php

/**
 * @file
 * Miscellaneous helper functions.
 */

/**
 * Returns issue object of a given page object.
 *
 * @param object $object
 *   Newspaper page object
 *
 * @return string
 *   Returns the PID of the issue object of which $object is a page of.
 */
function islandora_newspaper_get_issue($object) {
  $rels = $object->relationships->get(ISLANDORA_RELS_EXT_URI, 'isPageOf');
  return $rels[0]['object']['value'];
}

/**
 * Returns the sequence number of a given newsletter page.
 *
 * @param object $object
 *   Newspaper page object
 *
 * @return string
 *   Returns the sequence number of a page in an issue.
 */
function islandora_newspaper_get_current_sequence($object) {
  $rels = $object->relationships->get(ISLANDORA_RELS_EXT_URI, 'isSequenceNumber');
  return $rels[0]['object']['value'];
}

/**
 * Returns the collection object of a given issue object.
 *
 * @param object $object
 *   Newspaper issue object
 *
 * @return string
 *   Returns the PID of the collection of which $object is a member of.
 */
function islandora_newspaper_get_newspaper($object) {
  $relationship = variable_get('islandora_newspaper_issue_relationship', 'isMemberOf');
  $rels = $object->relationships->get(FEDORA_RELS_EXT_URI, $relationship);
  return $rels[0]['object']['value'];
}

/**
 * Gets all the issues that the given newspaper owns.
 *
 * The results are ordered by their RELS-EXT dateIssued property.
 * Older to newer.
 *
 * @param AbstractObject $object
 *   The datastream containing MODS.
 *
 * @return array
 *   An associative array in the form of:
 *   - pid: The unique persistent identifier for the issue.
 *     - pid: The unique persistent identifier for the issue.
 *     - label: A descriptive label for the issue.
 *     - sequence: The sequence number of the issue, starts at 1.
 *     - issued: A DateTime object repersenting the date the issue was released.
 */
function islandora_newspaper_get_issues(AbstractObject $object) {
  $relationship = variable_get('islandora_newspaper_issue_relationship', 'isMemberOf');

  $solr_results = islandora_newspaper_get_solr_issues($object->id);
  $results = array();

  foreach($solr_results['response']['docs'] as $doc) {
    $results[$doc['PID']] = array(
      'pid' => $doc['PID'],
      'label' => $doc['fgs_label_s'],
      'sequence' => '',
      @'issued' => new DateTime($doc['RELS_EXT_dateIssued_literal_ms'][0]),
    ) ;
  }
  return $results;
}


function islandora_newspaper_get_solr_issues($parent) {
  $solr_location = variable_get('islandora_solr_url', 'http://localhost:8080/solr');
  $solr_handler = variable_get('islandora_solr_request_handler', FALSE);
  $path_parts = parse_url(variable_get('islandora_solr_url', 'localhost:8080/solr'));
  $solr = new Apache_Solr_Service($path_parts['host'], $path_parts['port'], $path_parts['path'] . '/');
  $solr->setCreateDocuments(0);
  $parent = $solr->escape($parent);
  $cmodel = $solr->escape('islandora:newspaperIssueCModel');
  $solr_query = "content_model_ms:$cmodel AND parent_pid:$parent";
  $solr_params = array();
  $solr_params['fl'][] = 'PID, RELS_EXT_dateIssued_literal_ms, fgs_label_s';
  $solr_params['sort'] = array('mods_originInfo_dateIssued_dt' . ' asc');
  if ($solr_handler) {
    $solr_params['qt'] = $solr_handler;
  }
  if ($namespaces_enforced) {
    $namespace_list = trim(variable_get('islandora_pids_allowed', ''));
    if ($namespace_list) {
      $namespaces = preg_split('/[,|\s]/', $namespace_list);
      $namespace_array = array();
      foreach (array_filter($namespaces) as $namespace) {
        $namespace_array[] = 'PID:' . Apache_Solr_Service::escape($namespace) . '*';
      }
      $solr_params['fq'][] = implode(' OR ', $namespace_array);
    }
  }

  try {
    $method = 'GET';
    $results = $solr->search($solr_query, $start, '100000', $solr_params, $method);
  }
  catch (Exception $e) {
    watchdog('islandora_xmlsitemap', 'Error getting solr results %e',
      array('%e' => $e->getMessage()));
  }
  $solr_results = NULL;
  if (!empty($results)) {
    $solr_results = json_decode($results->getRawResponse(), TRUE);
  }
  return $solr_results;
}


/**
 * Groups the issues by their relvant dates.
 *
 * @param array $issues
 *   Expected to be in the format as returned by islandora_newspaper_get_issues.
 *
 * @return array
 *   An associative array where the issues are grouped like so:
 *   - 1989: The year of the grouped issues.
 *     - 01: The month of the grouped issues.
 *       - 28: The day of the grouped issues contains actual issue descriptions.
 */
function islandora_newspaper_group_issues(array $issues) {
  $output = array();
  foreach ($issues as $issue) {
    $year = $issue['issued']->format('Y');
    $month = $issue['issued']->format('m');
    $day = $issue['issued']->format('d');
    $output[$year][$month][$day] = $issue;
  }
  return $output;
}

/**
 * Attempts to get the date issued from the mods document.
 *
 * @param AbstractDatastream $datastream
 *   The datastream containing MODS.
 *
 * @return DateTime
 *   The date issued if found, otherwise the current time.
 */
function islandora_newspaper_get_date_issued_from_mods(AbstractDatastream $datastream) {
  $out = FALSE;
  $file = file_create_filename("{$datastream->parent->id}_{$datastream->id}.xml", 'temporary://');
  $datastream->getContent($file);
  @$doc = simplexml_load_file($file);
  if ($doc) {
    $doc->registerXPathNamespace('ns', 'http://www.loc.gov/mods/v3');
    // Assumes the canonical date issued exists in the first mods document under
    // origin info and is not specified as a point, additional logic could be
    // added to process different encodings.
    $dates = $doc->xpath('//ns:mods[1]/ns:originInfo/ns:dateIssued[not(@point)][1]');
    $result = (string) reset($dates);
    try {
      if (empty($result)) {
        throw new Exception('mods:dateIssued element was empty.');
      }
      $out = new DateTime($result);
    }
    catch (Exception $e) {
      $msg = 'Failed to get issued date from MODS for @pid';
      $vars = array('@pid' => $datastream->parent->id);
      watchdog_exception('islandora_newspaper', $e, $msg, $vars, WATCHDOG_ERROR);
    }
  }
  file_unmanaged_delete($file);
  return $out;
}

/**
 * Set's the date issued literal in the objects RELS-EXT.
 *
 * @param AbstractObject $object
 *   Newspaper issue object
 * @param DateTime $date
 *   The date issued, will be stored in ISO-8601 YYYY-MM-DD format.
 */
function islandora_newspaper_set_date_issued(AbstractObject $object, DateTime $date) {
  $object->relationships->remove(ISLANDORA_RELS_EXT_URI, 'dateIssued', NULL, TRUE);
  $object->relationships->add(ISLANDORA_RELS_EXT_URI, 'dateIssued', $date->format("Y-m-d"), TRUE);
}

/**
 * Fetchs the date issued from the objects RELS-EXT.
 *
 * If the date issue is not defined the current time is returned.
 *
 * @param AbstractObject $object
 *   Newspaper issue object
 *
 * @return DateTime
 *   The date issued. If the date issue is not defined the current time is used.
 */
function islandora_newspaper_get_date_issued(AbstractObject $object) {
  $results = $object->relationships->get(ISLANDORA_RELS_EXT_URI, 'dateIssued', NULL, TRUE);
  $date = empty($results) ? new DateTime() : new DateTime($results[0]['object']['value']);
  return $date;
}
